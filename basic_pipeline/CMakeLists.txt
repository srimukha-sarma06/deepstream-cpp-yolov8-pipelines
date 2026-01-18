cmake_minimum_required(VERSION 3.10)
project(ds_pipeline)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GStreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED
    gstreamer-1.0
    gstreamer-video-1.0
    gstreamer-base-1.0
)

# Include paths
include_directories(
    ${GST_INCLUDE_DIRS}
    /opt/nvidia/deepstream/deepstream/sources/includes
)

# Library paths
link_directories(
    ${GST_LIBRARY_DIRS}
    /opt/nvidia/deepstream/deepstream/lib
)

# Your executable
add_executable(ds_pipeline main.cpp)

# Link DeepStream + GStreamer libs
target_link_libraries(ds_pipeline
    ${GST_LIBRARIES}
    nvdsgst_meta
    nvds_meta
    nvds_utils
    nvbufsurface
    nvbufsurftransform
)
