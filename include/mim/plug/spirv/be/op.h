#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include "fe/assert.h"

// Enums
namespace mim::plug::spirv {
using Word = int32_t;

using namespace std::string_literals;

std::vector<Word> string_to_words(std::string_view string);

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

namespace storage_class {
enum StorageClass {
    Input   = 1,
    Uniform = 2,
    Output  = 3,
};

std::string name(int storage_class);
} // namespace storage_class

namespace decoration {
enum Decoration {
    Block    = 2,
    BuiltIn  = 11,
    Location = 30,
};

std::string name(int decoration);
} // namespace decoration

namespace builtin {
enum BuiltIn {
    Position      = 0,
    PointSize     = 1,
    VertexId      = 5,
    InstanceId    = 6,
    VertexIndex   = 42,
    InstanceIndex = 43,
    FragCoord     = 15,
};

std::string name(int builtin);
} // namespace builtin

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
    Decorate,
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
    TypePointer,
    Variable,
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
            case OpKind::Decorate: return "OpDecorate";
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
            case OpKind::TypePointer: return "OpTypePointer";
            case OpKind::Variable: return "OpVariable";
            case OpKind::Constant: return "OpConstant";
            case OpKind::ConstantComposite: return "OpConstantComposite";
            case OpKind::CompositeConstruct: return "OpCompositeConstruct";
            default: fe::unreachable();
        }
    }
};
} // namespace mim::plug::spirv
