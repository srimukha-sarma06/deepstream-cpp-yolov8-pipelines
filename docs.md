# DeepStream Pipeline Architectures

This document describes the design and data flow of the two DeepStream pipelines implemented in this repository:

* **Basic Pipeline** – Single-stream YOLOv8 inference with MP4 file output
* **Multi-Stream Pipeline** – Four-source batched inference with demux and RTSP fan-out

Both pipelines are implemented in **C++ using GStreamer and NVIDIA DeepStream SDK** and leverage **TensorRT for GPU-accelerated inference**.

---

## 1. Basic Pipeline (Single Stream)

### Overview

The basic pipeline is designed for **single-source real-time inference and video recording**. It performs video ingestion, YOLOv8-based object detection, on-screen display (OSD), and hardware-accelerated encoding to produce an MP4 output file.

This pipeline is suitable for:

* Model validation and debugging
* Performance benchmarking
* Single-camera deployments
* Offline analytics and dataset generation

---

### Pipeline Architecture (File Output Path)

```
Source (nvurisrcbin)
  ↓
nvstreammux
  ↓
nvinfer (YOLOv8)
  ↓
nvmultistreamtiler
  ↓
nvvideoconvert
  ↓
nvdsosd
  ↓
capsfilter
  ↓
nvv4l2h264enc
  ↓
h264parse
  ↓
qtmux
  ↓
filesink
```

---

### Data Flow Summary

* Input is ingested using `nvurisrcbin`, which handles source abstraction and hardware-accelerated decoding.
* `nvstreammux` standardizes the pipeline interface and enables future scalability to multi-source batching.
* `nvinfer` runs YOLOv8 inference using TensorRT and attaches detection metadata to GPU buffers.
* `nvmultistreamtiler` composes the stream into a display-friendly layout.
* `nvdsosd` renders bounding boxes and class labels using `NvDsObjectMeta`.
* `capsfilter` enforces encoder-compatible video format and GPU memory type (NVMM).
* `nvv4l2h264enc` performs hardware-accelerated H.264 encoding.
* `h264parse` ensures correct bitstream formatting for containerization.
* `qtmux` packages the encoded stream into an MP4 container.
* `filesink` writes the final output video to disk.

---

### Design Characteristics

* End-to-end GPU processing to minimize CPU-GPU memory transfers
* Deterministic output format for reproducible testing
* Modular structure that can be extended to live streaming or multi-source batching

---

## 2. Multi-Stream Pipeline (4-Source Demux + RTSP Fan-Out)

### Overview

The multi-stream pipeline is designed for **real-time multi-camera analytics systems**. It performs **batched GPU inference across four input sources**, then **demultiplexes the batch** and serves each source as an **independent RTP/UDP stream**, which can be exposed as an RTSP endpoint using an external RTSP server.

This architecture reflects production deployments used in:

* Smart surveillance platforms
* Traffic monitoring systems
* Industrial inspection lines
* Distributed video analytics services

---

### High-Level Design

Frames from four independent sources are decoded and batched into a single GPU buffer using `nvstreammux`. Inference is performed once per batch for maximum GPU utilization. The results are then split back into per-source streams using `nvstreamdemux`, enabling independent processing, encoding, and streaming for each source.

---

### Pipeline Architecture (4 Branches)

```
Source 0 ─┐
Source 1 ─┼─> Decode ─> nvstreammux ─> nvinfer (YOLOv8) ─> nvstreamdemux
Source 2 ─┼─────────────────────────────────────────────────────────────┐
Source 3 ─┘                                                             │
                                                                         ├─ Branch 0 → queue → nvvideoconvert → capsfilter → nvdsosd → nvvideoconvert → capsfilter → nvv4l2h264enc → rtph264pay → udpsink (RTSP 0)
                                                                         ├─ Branch 1 → queue → nvvideoconvert → capsfilter → nvdsosd → nvvideoconvert → capsfilter → nvv4l2h264enc → rtph264pay → udpsink (RTSP 1)
                                                                         ├─ Branch 2 → queue → nvvideoconvert → capsfilter → nvdsosd → nvvideoconvert → capsfilter → nvv4l2h264enc → rtph264pay → udpsink (RTSP 2)
                                                                         └─ Branch 3 → queue → nvvideoconvert → capsfilter → nvdsosd → nvvideoconvert → capsfilter → nvv4l2h264enc → rtph264pay → udpsink (RTSP 3)
```

---

### Element Breakdown

| Element            | Purpose                                                                               |
| ------------------ | ------------------------------------------------------------------------------------- |
| **nvstreammux**    | Batches frames from multiple sources into a single GPU buffer for efficient inference |
| **nvinfer**        | Runs YOLOv8 inference on batched frames using TensorRT and custom parser              |
| **nvstreamdemux**  | Splits the batched output back into individual per-source streams                     |
| **queue**          | Isolates branch latency and prevents backpressure across streams                      |
| **nvvideoconvert** | Converts pixel format and memory layout as required by downstream elements            |
| **capsfilter**     | Enforces format and memory constraints for OSD and encoder compatibility              |
| **nvdsosd**        | Renders bounding boxes and labels per stream                                          |
| **nvv4l2h264enc**  | Hardware-accelerated H.264 encoder                                                    |
| **rtph264pay**     | Packetizes H.264 stream into RTP packets                                              |
| **udpsink**        | Sends RTP packets over UDP for RTSP serving                                           |

---

### Design Principles

#### Batched Inference

* Maximizes GPU utilization by processing multiple streams simultaneously
* Reduces per-stream inference overhead
* Improves overall system throughput

#### Per-Branch Isolation

* Each output stream runs in its own branch after demultiplexing
* `queue` elements prevent slow consumers from blocking other streams
* Allows independent caps negotiation and encoder tuning

#### Hardware Acceleration

* Uses NVIDIA hardware decoders and encoders
* Keeps buffers in GPU memory (NVMM) as long as possible
* Minimizes end-to-end latency

---

### RTSP Integration

The pipeline outputs **RTP/H.264 streams over UDP**. These can be exposed as RTSP endpoints using an external RTSP server such as:

* `gst-rtsp-server`
* Live555

#### Example Endpoints

```
rtsp://<host-ip>:8554/stream0
rtsp://<host-ip>:8554/stream1
rtsp://<host-ip>:8554/stream2
rtsp://<host-ip>:8554/stream3
```

---

## 3. Performance Considerations

* **Batch size tuning** in `nvstreammux` directly impacts latency vs throughput
* GPU memory bandwidth becomes a bottleneck at higher resolutions and stream counts
* Encoder presets affect both visual quality and end-to-end latency
* Network bandwidth must be provisioned for multiple concurrent RTP streams

---

## 4. Engineering Highlights

* Custom YOLO TensorRT bounding box parser integration
* Dynamic request pad handling for multi-source and demux elements
* Branch-level pipeline construction using reusable builders
* Metadata extraction using `NvDsBatchMeta` and `NvDsObjectMeta`
* Containerized development and deployment environment for reproducibility

---

## 5. Future Extensions

* Dynamic source add/remove at runtime
* Adaptive bitrate streaming per branch
* Multi-model inference (detection + tracking + classification)
* Web-based RTSP stream management dashboard
