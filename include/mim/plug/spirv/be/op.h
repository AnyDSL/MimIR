#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include "fe/assert.h"

// Enums
namespace mim::plug::spirv {
using Word = int32_t;

using namespace std::string_literals;

namespace capability {
enum Capability {
    Shader = 1,
    Kernel = 6,
};

std::string name(int capability);
} // namespace capability

namespace addressing_model {
enum AddressingModel {
    Logical    = 0,
    Physical32 = 1,
    Physical64 = 2,
};

std::string name(int model);
} // namespace addressing_model

namespace memory_model {
enum MemoryModel {
    Simple  = 0,
    GLSL450 = 1,
    OpenCL  = 2,
    Vulkan  = 3,
};

std::string name(int model);
} // namespace memory_model

namespace execution_model {
enum ExecutionModel {
    Vertex    = 0,
    Fragment  = 4,
    GLCompute = 5,
};

std::string name(int model);
} // namespace execution_model

namespace function_control {
enum FunctionControl {
    None       = 0,
    Inline     = 1,
    DontInline = 2,
    Pure       = 4,
    Const      = 8,
};

std::string name(int control);
} // namespace function_control

namespace ext_inst {
enum ExtInstSet {
    GLSLstd450 = 1,
};

std::string name(int set);
} // namespace ext_inst

enum class OpKind {
    Capability,
    Extension,
    ExtInstImport,
    MemoryModel,
    EntryPoint,
    ExecutionMode,
    Decoration,
    Decl,
    Function,
    FunctionParameter,
    FunctionEnd,
    FunDef,
    Type,
    TypeVoid,
    TypeFunction,
    TypeInt,
    TypeFloat,
    TypeVector,
    TypeArray,
    TypeStruct,
    Constant,
    ConstantComposite,
    CompositeConstruct,
};
} // namespace mim::plug::spirv

// Op
namespace mim::plug::spirv {
struct Op {
    OpKind kind;
    std::vector<Word> operands;
    std::optional<Word> result;
    std::optional<Word> result_type;

    constexpr const char* name() {
        switch (this->kind) {
            case OpKind::Capability: return "OpCapability";
            case OpKind::Extension: return "OpExtension";
            case OpKind::ExtInstImport: return "OpExtInstImport";
            case OpKind::MemoryModel: return "OpMemoryModel";
            case OpKind::EntryPoint: return "OpEntryPoint";
            case OpKind::ExecutionMode: return "OpExecutionMode";
            case OpKind::Decoration: return "OpDecoration";
            case OpKind::Decl: return "OpDecl";
            case OpKind::Function: return "OpFuntion";
            case OpKind::FunctionParameter: return "OpFunctionParameter";
            case OpKind::FunctionEnd: return "OpFunctionEnd";
            case OpKind::Type: return "OpType";
            case OpKind::TypeVoid: return "OpTypeVoid";
            case OpKind::TypeFunction: return "OpTypeFunction";
            case OpKind::TypeInt: return "OpTypeInt";
            case OpKind::TypeFloat: return "OpTypeFloat";
            case OpKind::TypeVector: return "OpTypeVector";
            case OpKind::TypeArray: return "OpTypeArray";
            case OpKind::TypeStruct: return "OpTypeStruct";
            case OpKind::Constant: return "OpConstant";
            case OpKind::ConstantComposite: return "OpConstantComposite";
            case OpKind::CompositeConstruct: return "OpCompositeConstruct";
            default: fe::unreachable();
        }
    }
};
} // namespace mim::plug::spirv
