# ICP 2026

Since we are using new model (ONNX model YuNet), without any optimalizator, it runs 5~ FPS, even on separate thread. 

What we need is to build a release version which is done by:

Going into VSCode:
- Ctrl+Shift+P
- CMake: Select Configure Preset
- Vyber vcpkg-release
- CMake: Configure
- CMake: Build

After that, the release build achieves 60+ FPS.
