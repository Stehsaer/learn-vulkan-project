# Learn-Vulkan-Project

## About This Repository

This repository contains code I write while learning Vulkan.

- Written in C++20.
- Works with GCC, MSVC and Clang.
- Structured using CMake with vcpkg as package manager.
- Needs to install Vulkan SDK from LunarG as prerequisite.

Trying my best to make my codebase clean and neat! :smile:

## Contents

- `./vklib-hpp`: My customized C++ wrapper of original Vulkan C++ wrapper from Khronos. Heavily utilizes RAII and ensures dependency at resource releasing and cleaning.

- `./projects`: Projects created while learning Vulkan.

  - `./projects/deferred-shading-gltf`: A program for rendering GLTF using deferred shading, utilizes and experiments with various real-time rendering techniques.

    **Features**:

    - Deferred Shading
    - Cascading Shadow Map
    - Histogram-based Auto Exposure Algorithm
    - Bounding-box Frustum Culling
    - Bloom PostFX
    - PBR Rendering based on BRDF
    - Image Based Environment Lighting
    - Runtime Generated IBL
    - GLTF Model Parsing and Rendering
    - Optimized Shadow Map View Adaptation
    - AgX Tonemapping

    **Planned Features**:

    - HDR Support
    - 10-bit Rendering Support
    - Multi-threaded Command Recording
    - Mesh Combination at GLTF Parsing Stage
    - Skins & Animation
    - KTX Loading & Texture Compression
    - Built-in Render Time Measuring

- `./decrepated`: Decrepated projects, created using C API at the very early stage of learning Vulkan.

## Assets Source

### Deferred Shading GLTF

- `builtin-hdr.hdr`: [*Industrial Sunset Puresky*](https://polyhaven.com/a/industrial_sunset_puresky) from Poly Haven's HDRi asset library.

- `damaged-helmet.glb`: *Damaged Helmet* from Khronos' [glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets) Github repository.

- `roboto.ttf`: From Googlefont releasing github repository [*Roboto*](https://github.com/googlefonts/roboto/releases).
