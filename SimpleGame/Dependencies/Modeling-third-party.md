# Model loader dependencies

Vendored headers; no build-time download or package manager is required.

| Dependency | Version | License | Source | Header SHA-256 |
|---|---|---|---|---|
| cgltf | v1.15 | MIT; `cgltf/LICENSE` | https://github.com/jkuhlmann/cgltf/tree/v1.15 | `e378a21c084bf1f288bb799de827bb26906efb024255f1ecf1705ea13f11c6ec` |
| JSON for Modern C++ | v3.12.0 | MIT; `nlohmann/LICENSE.MIT` | https://github.com/nlohmann/json/tree/v3.12.0 | `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63` |

`CGLTF_IMPLEMENTATION` is defined only in `ModelAsset.cpp`. The MSVC unsafe-CRT warning suppression is limited to that third-party implementation. Project code uses `_dupenv_s` for environment variables.

Format reference: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
