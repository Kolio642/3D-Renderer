# 3DRenderer

A modern, real-time 3D renderer built in **C++17** utilizing the next-generation **SDL3 GPU API** and **Metal Shading Language (MSL)** on macOS.

---

## Overview

**3DRenderer** demonstrates how to build a clean, modern 3D graphics application using SDL3's high-performance hardware-accelerated GPU abstraction layer (`SDL_GPUDevice`). Designed to move away from legacy graphics pipelines, it runs custom Metal shaders directly on Apple Silicon and Intel Macs with low overhead, programmable pipelines, and automated resource binding.

The renderer features Wavefront `.obj` model loading with multi-material support (`.mtl`), texture mapping, vertex deduplication, directional Lambertian lighting, dynamic depth buffering, and an interactive 6DoF first-person camera.

---

## Features

- **SDL3 GPU Abstraction**: Direct hardware rendering via `SDL_GPUDevice` utilizing the Metal backend (`SDL_GPU_SHADERFORMAT_MSL`).
- **Programmable Metal Shaders**: Clean vertex and fragment shaders written in MSL (`shaders/triangle.msl`) with vertex attribute inputs, uniform buffers, and sampler bindings.
- **Wavefront OBJ & MTL Support**: Loads multi-mesh scenes and parses materials with diffuse colors and texture maps using `tiny_obj_loader`.
- **Submesh Batching & Material Splitting**: Groups draw calls into submeshes based on material ID, binding the respective diffuse texture or a generated 1x1 white fallback texture.
- **Vertex Deduplication**: Hashes and indexes unique vertex attributes (position, normal, UV, material ID) to minimize memory footprint and optimize GPU vertex caches.
- **Model Centering & Normalization**: Automatically calculates axis-aligned bounding boxes (AABB) to center and scale loaded models to fit within the viewport.
- **Directional & Ambient Lighting**: Real-time Lambertian diffuse shading combined with ambient light in the fragment stage.
- **Dynamic Depth Testing**: 32-bit floating-point depth buffer (`SDL_GPU_TEXTUREFORMAT_D32_FLOAT`) with automatic recreation and aspect ratio updates upon window resizing.
- **6DoF Free-Fly Camera**: First-person camera with mouse-look pitch/yaw clamping, forward/strafe/vertical motion, and sprint boost.

---

## Controls

Click inside the window to lock the cursor into first-person look mode.

| Input | Action |
| :--- | :--- |
| **Left Click** | Capture mouse and enable camera look |
| **Mouse Move** | Look around (Pitch & Yaw) |
| **Escape (`Esc`)** | Release mouse capture |
| **`W` / `S`** | Move Forward / Backward |
| **`A` / `D`** | Strafe Left / Right |
| **`Space`** | Ascend (Fly Up) |
| **`Left Shift`** | Descend (Fly Down) |
| **`Left Ctrl`** | Sprint (2.5x speed boost) |

---

## Tech Stack & Dependencies

- **Language**: C++17
- **Graphics & Windowing**: [SDL3](https://github.com/libsdl-org/SDL) (SDL_GPU API)
- **Shader Language**: Metal Shading Language (MSL)
- **Math Library**: [GLM](https://github.com/g-truc/glm) (OpenGL Mathematics)
- **Mesh Loading**: [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) (Header-only)
- **Image Loading**: [stb_image](https://github.com/nothings/stb) (Header-only)
- **Build System**: CMake 3.24+

---

## Project Structure

```text
3DRenderer/
├── CMakeLists.txt          # CMake configuration and shader copy target
├── main.cpp                # Application entry point
├── RenderApp.h             # Main renderer class definition & vertex structures
├── RenderApp.cpp           # SDL3 GPU pipeline, mesh setup, and render loop
├── Camera.h                # First-person view & projection camera class
├── tiny_obj_loader.h       # OBJ model parser
├── stb_image.h             # Image loader for textures
├── shaders/
│   └── triangle.msl        # Metal vertex & fragment shaders
├── resources/
│   ├── breakfast_room/     # Multi-material architectural interior scene
│   └── dragon.obj          # Stanford Dragon 3D mesh
└── .vscode/                # VS Code build tasks, launch configs, and Metal LSP settings
```

---

## Getting Started

### Prerequisites

This project is tailored for macOS with Metal support. Ensure you have Xcode Command Line Tools installed:

```bash
xcode-select --install
```

Install **SDL3**, **GLM**, and **CMake** using [Homebrew](https://brew.sh/):

```bash
brew install sdl3 glm cmake
```

### Build and Run

1. **Clone or navigate to the repository**:
   ```bash
   cd /path/to/3DRenderer
   ```

2. **Configure with CMake**:
   ```bash
   cmake -B build
   ```

3. **Compile the executable**:
   ```bash
   cmake --build build
   ```
   *(Note: The build process automatically copies the `shaders/` directory next to the output binary).*

4. **Launch the application**:
   ```bash
   ./build/3DRenderer
   ```

---

## Development & VS Code Integration

Pre-configured `.vscode/` configurations are included in the repository:
- **Build Task**: Press `Cmd + Shift + B` to trigger the `CMake Build` task.
- **Configure Task**: Run `CMake Configure` to regenerate build files and `compile_commands.json`.
- **IntelliSense**: Configured for `clang-arm64` with Homebrew and macOS framework include paths.
- **Metal LSP**: Syntax highlighting and language association configured for `.msl` files.

---

## Assets & Attributions

- **Breakfast Room Scene**: Created by Wig42 (Creative Commons Attribution 3.0), converted and materials corrected by Morgan McGuire & Bitterli.
- **Stanford Dragon**: Stanford Computer Graphics Laboratory.
