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
    - Multi-threaded Command Recording
    - Debug Marker
    - Dither-based Alpha Rendering
    - 10-bit Rendering Support

    **Planned Features**:

    - HDR Support
    - Mesh Combination at GLTF Parsing Stage
    - Skins & Animation
    - KTX Loading & Texture Compression
    - Built-in Render Time Measuring

- `./decrepated`: Decrepated projects, created using C API at the very early stage of learning Vulkan.

## Third-party Assets and Licenses

### Roboto Font

The Roboto font is used in this project under the Apache License 2.0. The font was obtained from the [Google Fonts repository](https://github.com/googlefonts/roboto/releases). A copy of the Apache License 2.0 can be found in the `licenses` directory alongside the font. The project also contains a copy of the license `licenses\apache-license-2-0.txt`.

Files:

- `projects/deferred-shading-gltf/assets/roboto.ttf`

### HDRi From Polyhaven

The asset "Industrial Sunset Puresky" is used in this project under CC0 license. The asset was obtained from [Polyhaven](https://polyhaven.com/a/industrial_sunset_puresky). A link to the CC0 license can be found in the ["licenses" page of Polyhaven](https://polyhaven.com/license). The project also contains a copy of the license `licneses/cc0-license.txt`. Great appreciations to all the work done by the creator and Polyhaven!

Files:

- `projects/deferred-shading-gltf/assets/builtin-hdr.hdr`

### GLTF Sample Assets

The binary asset "Damanged Helmet" is used in this project under various licenses. The asset was obtained from [gltf-Sample-Assets repository from Github](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/DamagedHelmet). A copy of the licenses can be found at `LICENSES.md` under the link where the asset was obtained. The project also contains copies of the licenses

- CC BY 4.0 International: `licenses\cc-by-4-0-international.txt`
- CC BY-NC 4.0 International: `licenses\cc-by-nc-4-0-international.txt`

Files:

- `projects/deferred-shading-gltf/assets/damaged-helmet.glb`

## Legal Notice

All copyrights of the contents except THIRD-PARTY ASSETS and THIRD-PARTY LIBRARIES are reserved by the owner of this repository. The copyrights of THIRD-PARTY ASSETS and THIRD-PARTY LIBRARIES belong to the original creators or belong to the ones that their licenses specify. However, this repository is meant to be for educational-purposes. Permission is granted for educational purposes, including school or college education and personal learning purposes. For more info, contact Hsin-chieh Liu via email: <stehsaer@outlook.com>.
