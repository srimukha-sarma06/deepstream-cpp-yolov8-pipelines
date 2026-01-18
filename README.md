DeepStream C++ YOLOv8 Pipelines

This repository contains NVIDIA DeepStream pipelines implemented in C++ using GStreamer, TensorRT, and custom YOLO parsers. It supports both single-stream and multi-stream inference using YOLOv8 models, designed for deployment on Jetson and NVIDIA dGPU systems.

The project is built and tested using NVIDIA’s official DeepStream Docker environment for reproducibility.

Repository Structure
basic_pipeline/
  ├── main.cpp                         # Single-stream DeepStream C++ pipeline
  ├── config_infer_primary_yolov8.txt # nvinfer YOLOv8 configuration
  └── CMakeLists.txt                 # Build configuration

multi_stream/
  ├── main.cpp                       # Multi-source batched pipeline (nvstreammux)
  ├── config_infer_primary_yolov8.txt
  └── CMakeLists.txt

deepstream_text_pipeline/
  ├── config_infer_primary_yolov8.txt
  ├── deepstream_app.txt            # Reference DeepStream app prototype config
  └── model_labels_10-06-2025.txt  # Class label file

Features

YOLOv8 inference using TensorRT (nvinfer)

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

CUDA 11.x+

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

DeepStream does not natively support YOLO models. This project uses a custom TensorRT YOLO parser from the DeepStream-Yolo repository.

Build Custom Parser

Inside the container:

cd /opt/nvidia/deepstream/deepstream/sources
git clone https://github.com/marcoslucianops/DeepStream-Yolo.git
cd DeepStream-Yolo
make -C nvdsinfer_custom_impl_Yolo

Update YOLO Config

In config_infer_primary_yolov8.txt, set:

custom-lib-path=nvdsinfer_custom_impl_Yolo/libnvdsinfer_custom_impl_Yolo.so
parse-bbox-func-name=NvDsInferParseYolo

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
