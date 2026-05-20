# Programowanie Obiektowe Projekt
> [!WARNING]
> Work in progress

## Objective

Project is an agentic interactive simulation featuring fish in an aquarium. Simulation consist of non-linear movements and collision events happening between fish.

## Dependencies
### Required
- Compiler with support for C++ 23
- VulkanSDK (graphics API)
- glm (math)
- SDL3 (window handling)
### Fetched
- VMA (vulkan memory allocator)
- xxhash (hashing)
- ktx (texture format)
- ImGUI (graphical interface)
- tinygltf (3d model parsing)

## How to run?

Due to constant developement, project currently only works if its run from the build directory, so the suggested workflow would look like:
```
git clone https://github.com/Genkunen/ProgramowanieObiektoweProjekt
cd ProgramowanieObiektoweProjekt
mkdir build && cd build
cmake ..
cmake --build . && ./ProgramowanieObiektoweProjekt
```
It's  highly recommended to use multithreaded builtds using either ninja or -j$(nproc) flag:
```
cmake .. -GNinja
or
cmake .. -j$(nproc)
```
Project is optimized and tested under clang++ 22.1.*, so its also recommended to use clang to compile it. To hint CMake to use clang, its enough to pass those flags to `cmake ..` command:
```
-DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
```

## Focus
Focal point of this project comes down to architecture and usage of GPU compute power. Simple simulation logic is used to show relations between up to tens of millions of fish swimming around and interacting with each other. <br>

## Current State
Simulation shows variable amout of objects being drawn and interacting with each other. The amount can be changed in GUI panel, by specyfing new amout and clicking a button alongside the input. In addition to that there is a left over background color test.
<br>
Current testing showed promising results in up to 3,000,000 objects on intel and amd **integrated** laptop GPUs and over 10,000,000 objects on a dedicated amd GPU, maintaining steady 30+ FPS (below 30ms per frame). Currently no „fish” are swimming, but since logic is so simple it can be implemented at the end, when all systems prove capable of handling the goal.
