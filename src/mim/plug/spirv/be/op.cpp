#include "mim/plug/spirv/be/op.h"

#include <iostream>

namespace mim::plug::spirv {

std::vector<Word> string_to_words(std::string_view string) {
    std::vector<Word> out{};
    int index = 0;
    Word word = 0;
    for (auto c : string) {
        word |= c << (8 * index);
        index = (index + 1) % 4;
        if (index == 0) {
            out.push_back(word);
            word = 0;
        }
    }
    // push the last partially filled word (if any) or add null terminator word
    out.push_back(word);
    return out;
}

std::string words_to_string(std::vector<Word> words) {
    std::string out;
    Word mask = (1 << 8) - 1;
    for (Word word : words) {
        for (int index = 0; index < 4; index++)
            if (auto c = (word >> index) & mask)
                out.push_back(static_cast<char>(c));
            else
                return out;
    }
    return out;
}

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

namespace execution_model {
std::string name(int model) {
    switch (model) {
        case Vertex: return "Vertex"s;
        case Fragment: return "Fragment"s;
        case GLCompute: return "GLCompute"s;
        default: fe::unreachable();
    }
}
} // namespace execution_model

namespace function_control {
std::string name(int control) {
    switch (control) {
        case None: return "None"s;
        case Inline: return "Inline"s;
        case DontInline: return "DontInline"s;
        case Pure: return "Pure"s;
        case Const: return "Const"s;
        default: fe::unreachable();
    }
}
} // namespace function_control

namespace storage_class {
std::string name(int storage_class) {
    switch (storage_class) {
        case Input: return "Input"s;
        case Uniform: return "Uniform"s;
        case Output: return "Output"s;
        default: fe::unreachable();
    }
}
} // namespace storage_class

namespace decoration {
std::string name(int decoration) {
    switch (decoration) {
        case Block: return "Block"s;
        case BuiltIn: return "BuiltIn"s;
        case Location: return "Location"s;
        default: fe::unreachable();
    }
}
} // namespace decoration

namespace builtin {
std::string name(int builtin) {
    switch (builtin) {
        case Position: return "Position"s;
        case PointSize: return "PointSize"s;
        case VertexId: return "VertexId"s;
        case InstanceId: return "InstanceId"s;
        case VertexIndex: return "VertexIndex"s;
        case InstanceIndex: return "InstanceIndex"s;
        case FragCoord: return "FragCoord"s;
        default: return std::to_string(builtin);
    }
}
} // namespace builtin

namespace ext_inst {
std::string name(int set) {
    switch (set) {
        case GLSLstd450: return "\"GLSL.std.450\""s;
        default: fe::unreachable();
    }
}
} // namespace ext_inst
} // namespace mim::plug::spirv
