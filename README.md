# VulkanPT
A C++ Raytracer using Vulkan. Using opensource libraries and frameworks with custom implementations of shaders, GI, reflections, etc.

Uses GLFW for windowing, vk_bootstrap for boilerplate, VMA for memory management.

## TODO
#### Sections of the ray tracer and their respective milestones

### Vulkan Foundation
- [x] instance
- [x] surface
- [x] physical device
- [x] logical device
- [x] VMA integration
- [x] swapchian
- [x] single-time command buffer
- [x] window (GLFW)
- [ ] camera input/movement
- [ ] gamepad input support

### Resources/Shaders
- [x] GLSL SPIR-V compilation
- [x] minimal graphics pipeline
- [ ] Scene Loading

### Ray Tracing
- [ ] HLSL SPIR-V compilation (DXC)
- [ ] BLAS/TLAS Builder
- [ ] RT pipeline + shader binding table
- [ ] lightweight render graph

### Tooling
- [ ] Dear ImGui + parameter registry
- [ ] JSON config serialization
- [ ] shader hot reload (file water + ShaderManager)
- [ ] Profiling (Tracy + Vulkan timestamp queries)

### Additional extentions (if I'm able)
- [ ] NRD
- [ ] RTXDI
