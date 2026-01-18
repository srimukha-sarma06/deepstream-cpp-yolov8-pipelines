Deepstream C++ YOLOv8 Pipelines

This repository contains NVIDIA DeepStream pipelines implemented in C++ using GStreamer, TensorRT, and custom YOLO parsers. It supports both single-stream and multi-stream inference using YOLOv8 models, designed for deployment on Jetson and NVIDIA dGPU systems.

The project is built and tested using NVIDIA’s official DeepStream Docker environment for reproducibility.

Repository Structure

basic_pipeline/
  ├── main.cpp                         # Single-stream DeepStream C++ pipeline
  ├── config_infer_primary_yolov8.txt # NVInfer YOLOv8 configuration
  └── CMakeLists.txt                  # Build configuration

multi_stream/
  ├── main.cpp                       # Multi-source batched pipeline (nvstreammux)
  ├── config_infer_primary_yolov8.txt
  └── CMakeLists.txt

deepstream_text_pipeline/
  ├── config_infer_primary_yolov8.txt
  ├── deepstream_app.txt            # Reference DeepStream app prototype config
  └── model_labels_10-06-2025.txt  # Class label file
  
Features

YOLOv8 inference using TensorRT (NVInfer)
Single-stream and multi-stream pipeline support
Batched processing using nvmultiurisrcbin
Dynamic GStreamer pad linking
Modular CMake-based build system
Docker-based reproducible environment

Requirements

NVIDIA GPU or Jetson device
NVIDIA Container Toolkit
Docker
NVIDIA DeepStream SDK 7.1+
CUDA 11.X+
TensorRT
Docker Environment (Recommended)
This project is developed and tested using NVIDIA’s official DeepStream container:
nvcr.io/nvidia/deepstream:7.1-gc-triton-devel
Pull Image
docker pull nvcr.io/nvidia/deepstream:7.1-gc-triton-devel
Run Container
From the repository root:

docker run -it --gpus all \
  -v $(pwd):/workspace \
  --net=host \
  nvcr.io/nvidia/deepstream:7.1-gc-triton-devel
YOLO Integration (Custom Parser Required)
DeepStream does not natively support YOLO models. This project uses a custom TensorRT YOLO parser from the DeepStream-YOLO repository.

Build Custom Parser
Inside the container:

cd /opt/nvidia/deepstream/deepstream/sources
git clone https://github.com/marcoslucianops/deepstream-yolo.git
cd deepstream-yolo
make -C nvdsinfer_custom_impl_yolo
Update YOLO Config
In config_infer_primary_yolov8.txt, set:

custom-lib-path=nvdsinfer_custom_impl_yolo/libnvdsinfer_custom_impl_yolo.so
parse-bbox-func-name=nvdsinferparseyolo
Build Instructions
Build Basic Pipeline
cd basic_pipeline
mkdir build && cd build
cmake ..
make -j$(nproc)
Build Multi-Stream Pipeline
cd multi_stream
mkdir build && cd build
cmake ..
make -j$(nproc)
Run
From the build directory:

./deepstream_app ../config_infer_primary_yolov8.txt


Model Statistics (YOLOv8)
Model: YOLOv8n (TensorRT)
Input Resolution: 640 × 640
Precision: FP16
Parameters: ~3.2M
Model Size: ~6.2 MB
Inference Backend: TensorRT (DeepStream NVInfer)
Custom Parser: DeepStream-YOLO
Performance Benchmarks
Device	Streams	FPS (per stream)	Total Throughput	Latency (avg)
RTX 3050 (6GB)	4	~150 FPS	~600 FPS	~6–8 ms
Benchmarks measured using DeepStream FPS probe after NVInfer with batched inference enabled in the multi-stream pipeline. Performance includes GPU decode, inference, OSD, and H.264 encoding.

Notes
Sample videos and TensorRT engine files are not included in this repository.
The deepstream_text_pipeline/ directory contains reference configuration and early-stage prototyping files used during development.
TensorRT engines will be generated automatically at first run and cached for faster startup.
Engineering Highlights
Custom YOLO TensorRT bounding box parser integration
GStreamer element creation using factory pattern
Dynamic request pad handling for multi-source pipelines
Batched inference optimization using nvstreammux
Metadata parsing using DeepStream SDK APIs
License
MIT License
