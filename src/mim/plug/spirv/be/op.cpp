#include "mim/plug/spirv/be/op.h"

#include <iostream>

namespace mim::plug::spirv {
namespace capability {
std::string name(int capability) {
    switch (capability) {
        case Shader: return "Shader"s;
        case Kernel: return "Kernel"s;
        default: std::cerr << "wtf\n"; fe::unreachable();
    }
}
} // namespace capability

namespace addressing_model {
std::string name(int model) {
    switch (model) {
        case Logical: return "Logical"s;
        case Physical32: return "Physical32"s;
        case Physical64: return "Physical64"s;
        default: fe::unreachable();
    }
}
} // namespace addressing_model

namespace memory_model {
std::string name(int model) {
    switch (model) {
        case Simple: return "Simple"s;
        case GLSL450: return "GLSL450"s;
        case OpenCL: return "OpenCL"s;
        case Vulkan: return "Vulkan"s;
        default: fe::unreachable();
    }
}
} // namespace memory_model

namespace ext_inst {
std::string name(int set) {
    switch (set) {
        case GLSLstd450: return "\"GLSL.std.450\""s;
        default: fe::unreachable();
    }
}
} // namespace ext_inst
} // namespace mim::plug::spirv
