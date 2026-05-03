# Vulkan SPIR-V 셰이더 에셋

이 폴더에 사전 컴파일된 SPIR-V 파일을 배치하세요.

```
mesh.vert.spv   — Vertex Shader (GLSL → SPIR-V)
mesh.frag.spv   — Fragment Shader (GLSL → SPIR-V)
```

## 컴파일 방법 (Windows 빌드 머신에서 실행)

```bash
glslc Engine/Shaders/Vulkan/mesh.vert -o mesh.vert.spv
glslc Engine/Shaders/Vulkan/mesh.frag -o mesh.frag.spv
```

컴파일된 `.spv` 파일을 이 디렉토리에 복사하면
Gradle이 APK 빌드 시 자동으로 `assets/Shaders/Vulkan/` 경로로 패키징합니다.
