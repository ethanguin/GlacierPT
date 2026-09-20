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
- [x] minimal graphics pipeline
- [ ] camera input/movement
- [ ] gamepad input support

### Scene
- [x] Minimal scene loading (spheres)
- [ ] Minimal scene lighting (directional and point)
- [ ] Scene Loading
- [ ] Area Lights
- [ ] HDRI

### Ray Tracing
- [x] HLSL SPIR-V compilation (DXC)
- [x] BLAS/TLAS Builder
- [x] RT pipeline + shader binding table
- [ ] lightweight render graph

### Shaders
- [x] Raygen
- [x] GGX
- [ ] Shadow Rays
- [ ] Reflection Rays
- [ ] Ambient Occlusion
- [ ] LTC (for area light sampling)
- [ ] Global Illumination
- [ ] Denoise (shadows/reflections)
- [ ] Transmission/Refraction
- [ ] Transparency (with updated shadow ray logic)
- [ ] Volumetrics (Henyey-Greenstein?)
#### Additional Optional ones
- [ ] Multiple importance sampling
- [ ] Subsurface Scattering (not random walk, diffusion approximation)
- [ ] DLSS for denoising
- [ ] HDR tonemapping
- [ ] Bloom

### Tooling
- [x] GLSL SPIR-V compilation
- [x] Screenshot Utility
- [ ] Dear ImGui + parameter registry
- [ ] JSON config serialization
- [ ] shader hot reload (file water + ShaderManager)
- [ ] Profiling (Tracy + Vulkan timestamp queries)

### Additional extentions (if I'm able)
- [ ] NRD
- [ ] ReSTIR DI
- [ ] ReSTIR GI
- [ ] SHaRC
