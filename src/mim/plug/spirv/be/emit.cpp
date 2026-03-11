#include "mim/plug/spirv/be/emit.h"

#include <iostream>
#include <optional>
#include <ostream>
#include <string>

#include <mim/plug/math/math.h>

#include "mim/def.h"
#include "mim/lam.h"
#include "mim/tuple.h"

#include "mim/be/emitter.h"

#include "mim/plug/core/core.h"
#include "mim/plug/mem/mem.h"
#include "mim/plug/sflow/autogen.h"
#include "mim/plug/sflow/tuple.h"
#include "mim/plug/spirv/autogen.h"
#include "mim/plug/spirv/be/op.h"
#include "mim/plug/vec/autogen.h"

#include "absl/container/flat_hash_map.h"
#include "fe/assert.h"

using namespace std::string_literals;

namespace mim::plug::spirv {

namespace core = mim::plug::core;
namespace math = mim::plug::math;

// Helper function to check if a type is a scalar type suitable for vectors
bool is_scalar_type(const Def* type) { return type->isa<Nat>() || Idx::isa(type) || math::isa_f(type); }

// Helper function to check if a value is constant
// copied from mim::core::ll
// TODO: this should maybe be in utils::be or something
bool is_const(const Def* def) {
    if (def->isa<Bot>()) return true;
    if (def->isa<Lit>()) return true;
    if (auto pack = def->isa_imm<Pack>()) return is_const(pack->arity()) && is_const(pack->body());

    if (auto tuple = def->isa<Tuple>()) {
        auto ops = tuple->ops();
        return std::ranges::all_of(ops, [](auto def) { return is_const(def); });
    }

    return false;
}

using OpVec = std::vector<Op>;

struct BB {
    Op label;
    DefMap<std::vector<Word>> phis;
    OpVec ops;
    OpVec tail;
    std::optional<Op> merge;
    Op end;
};

class Emitter : public mim::Emitter<Word, Word, BB, Emitter> {
public:
    using Super = mim::Emitter<Word, Word, BB, Emitter>;

    Emitter(World& world, std::ostream& ostream)
        : Super(world, "spirv_emitter", ostream) {}

    bool is_valid(Word _) { return true; }
    void start() override {
        Super::start();

        capabilities.emplace_back(Op{OpKind::Capability, {capability::Shader}, {}, {}});

        glsl_ext_inst_id_ = next_id();
        extInstImports.emplace_back(Op{OpKind::ExtInstImport, {ext_inst::GLSLstd450}, glsl_ext_inst_id_, {}});
        id_names[glsl_ext_inst_id_] = "GLSL_std_450";

        memoryModel = Op{
            OpKind::MemoryModel,
            {addressing_model::Logical, memory_model::GLSL450},
            {},
            {}
        };
    }

    Word prepare();
    void emit_imported(Lam*) {}
    void emit_epilogue(Lam*);
    Word emit_bb(BB&, const Def*);
    void finalize();

    // Returns an optional name for an identifier to make
    // assembly more readable.
    std::string id_name(Word id) {
        auto it = id_names.find(id);
        if (it != id_names.end()) return it->second;
        return std::to_string(id);
    }

    void assembly() {
        ostream() << "; SPIR-V\n"
                  << "; Version: 1.0\n"
                  << "; Generator: MimIR; 0\n"
                  << "; Bound: " << next_id() << "\n"
                  << "; Schema: " << 0 << "\n";

        auto indent = max_name_length() + 4;

        assembly(indent, capabilities);
        assembly(indent, extensions);
        assembly(indent, extInstImports);
        assembly(indent, memoryModel);
        assembly(indent, entryPoints);
        assembly(indent, executionModes);
        assembly(indent, debug);
        assembly(indent, annotations);
        assembly(indent, declarations);
        assembly(indent, funDeclarations);
        assembly(indent, funDefinitions);
    }

    void binary() {}

private:
    const Def* strip(const Def*);
    const Def* strip_rec(const Def*);

    // Converts a type into a spirv type referenced by the
    // returned id.
    Word convert(const Def* type, std::string_view name = "");
    Word convert_ret_pi(const Pi* type);
    Word emit_interface(std::string name, const Def* global);
    void emit_decoration(Word var_id, const Def* decoration_);

    // Returns a new unique id. To make ids as low as possible, as
    // recommended by the spirv spec, they are counted up instead of
    // reusing MimIR ids.
    Word next_id() { return next_id_++; }

    /// Associates every unique value with an id Word.
    Word id(const Def* def) {
        // check if already assigned
        if (auto i = ids.find(def); i != ids.end()) return i->second;

        // create new id
        Word id      = next_id();
        ids[def]     = id;
        id_names[id] = def->unique_name();

        return id;
    }

    // TODO: move this to a separate file/class
    void assembly(int indent, Op& op) {
        // result
        if (op.result.has_value()) {
            auto result = id_name(op.result.value());
            for (int i = result.length() + 4; i < indent; i++)
                ostream() << " ";
            ostream() << "%" << result << " = ";
        } else {
            for (int i = 0; i < indent; i++)
                ostream() << " ";
        }

        // op
        ostream() << op.name();

        // result type (if present)
        if (op.result_type.has_value()) ostream() << " %" << id_name(op.result_type.value());

        switch (op.kind) {
            case OpKind::Capability: ostream() << " " << capability::name(op.operands[0]); break;
            case OpKind::ExtInstImport: ostream() << " " << ext_inst::name(op.operands[0]); break;
            case OpKind::MemoryModel:
                ostream() << " " << addressing_model::name(op.operands[0]);
                ostream() << " " << memory_model::name(op.operands[1]);
                break;
            case OpKind::EntryPoint: {
                auto ops = op.operands.cbegin();
                ostream() << " " << execution_model::name(*ops++);
                ostream() << " %" << id_name(*ops++);
                // TODO: investigate what is going slightly wrong here
                auto name = words_to_string(ops, op.operands.cend());
                ostream() << " \"" << name << "\"";
                for (; ops != op.operands.cend(); ops++)
                    ostream() << " %" << id_name(*ops);
                break;
            }
            case OpKind::ExecutionMode:
                ostream() << " %" << id_name(op.operands[0]);
                ostream() << " " << execution_mode::name(op.operands[1]);
                // Some execution modes have additional literal operands
                for (size_t i = 2; i < op.operands.size(); ++i)
                    ostream() << " " << op.operands[i];
                break;
            case OpKind::Function:
                ostream() << " " << function_control::name(op.operands[0]);
                ostream() << " %" << id_name(op.operands[1]);
                break;
            case OpKind::Decorate:
                ostream() << " %" << id_name(op.operands[0]);
                ostream() << " " << decoration::name(op.operands[1]);
                if (op.operands[1] == decoration::BuiltIn && op.operands.size() > 2) {
                    ostream() << " " << spv_builtin::name(op.operands[2]);
                    for (size_t i = 3; i < op.operands.size(); ++i)
                        ostream() << " " << op.operands[i];
                } else {
                    for (size_t i = 2; i < op.operands.size(); ++i)
                        ostream() << " " << op.operands[i];
                }
                break;
            case OpKind::TypePointer:
                ostream() << " " << storage_class::name(op.operands[0]);
                ostream() << " %" << id_name(op.operands[1]);
                break;
            case OpKind::Variable:
                ostream() << " " << storage_class::name(op.operands[0]);
                if (op.operands.size() > 1) // optional initializer
                    ostream() << " %" << id_name(op.operands[1]);
                break;
            case OpKind::TypeInt:
                ostream() << " " << op.operands[0]; // width
                ostream() << " " << op.operands[1]; // signedness: 0=unsigned, 1=signed
                break;
            case OpKind::TypeFloat:
                ostream() << " " << op.operands[0]; // width
                break;
            case OpKind::Constant:
                // hints[0] = type (0=int, 1=float), hints[1] = width
                if (op.hints.size() >= 2 && op.hints[0] == 1) {
                    ostream() << " " << words_to_float(op.operands, op.hints[1]);
                } else if (op.hints.size() >= 2) {
                    ostream() << " " << words_to_int(op.operands, op.hints[1]);
                } else {
                    // Fallback for constants without hints
                    ostream() << " " << op.operands[0];
                }
                break;
            case OpKind::TypeVector:
                ostream() << " %" << id_name(op.operands[0]); // component type
                ostream() << " " << op.operands[1];           // component count
                break;
            case OpKind::TypeArray:
                ostream() << " %" << id_name(op.operands[0]); // element type
                ostream() << " %" << id_name(op.operands[1]); // length (constant)
                break;
            case OpKind::CompositeExtract:
                ostream() << " %" << id_name(op.operands[0]); // composite
                ostream() << " " << op.operands[1];           // index
                break;
            case OpKind::TypeStruct:
            case OpKind::TypeFunction:
            case OpKind::ConstantComposite:
            case OpKind::CompositeConstruct:
                // All member/parameter/constituent types/values
                for (Word operand : op.operands)
                    ostream() << " %" << id_name(operand);
                break;
            case OpKind::FunctionParameter:
            case OpKind::FunctionEnd:
            case OpKind::TypeVoid:
                // No operands
                break;
            default:
                for (Word operand : op.operands)
                    ostream() << " %" << id_name(operand);
        }

        ostream() << "\n";
    }

    void assembly(int indent, OpVec& ops) {
        for (Op op : ops)
            assembly(indent, op);
    }

    int max_name_length() {
        int max = 0;
        for (auto name : id_names) {
            int length = name.second.length();
            if (length > max) max = length;
        }
        return max;
    }

    OpVec capabilities;
    OpVec extensions;
    OpVec extInstImports;
    Op memoryModel;
    OpVec entryPoints;
    OpVec executionModes;
    OpVec debug;
    OpVec annotations;
    OpVec declarations;
    OpVec funDeclarations;
    OpVec funDefinitions;

    DefMap<Word> ids;
    absl::flat_hash_map<Word, std::string> id_names;

    Word next_id_{0};
    Word glsl_ext_inst_id_{0};
    std::optional<Word> bool_type_id_{}; // Shared Bool type (OpTypeBool)
    std::optional<Word> i32_type_id_{};  // Shared 32-bit unsigned integer type for all Nat/Idx

    /// Get or create the SPIR-V bool type (for comparisons and branch conditions)
    Word bool_type() {
        if (bool_type_id_.has_value()) return *bool_type_id_;
        Word id = next_id();
        declarations.emplace_back(Op{OpKind::TypeBool, {}, id, {}});
        id_names[id]  = "bool";
        bool_type_id_ = id;
        return id;
    }
    absl::flat_hash_map<std::pair<Word, Word>, Word> ptr_types_{}; // (storage_class, type_id) -> pointer_type_id
    DefMap<Word> interface_vars_{};
};

const Def* Emitter::strip(const Def* def) {
    auto stripped = strip_rec(def);
    return stripped ? stripped : (def->type()->isa<Type>() ? (const Def*)def->world().sigma() : def->world().tuple());
}

/// Used by [strip]
const Def* Emitter::strip_rec(const Def* def) {
    auto& world = def->world();

    if (auto sigma = def->isa<Sigma>()) {
        DefVec fields{};
        for (auto field : sigma->ops())
            if (auto stripped = strip_rec(field)) fields.push_back(stripped);

        return world.sigma(fields);
    }

    if (auto sigma = def->isa<Tuple>()) {
        DefVec fields{};
        for (auto field : sigma->ops())
            if (auto stripped = strip_rec(field)) fields.push_back(stripped);

        return world.tuple(fields);
    }

    if (auto arr = def->isa<Arr>()) {
        if (auto body = strip_rec(arr->body()))
            return world.arr(arr->arity(), body);
        else
            return nullptr;
    }

    if (auto pack = def->isa<Pack>()) {
        if (auto body = strip_rec(pack->body()))
            return world.pack(pack->arity(), body);
        else
            return nullptr;
    }

    if (auto pi = def->isa<Pi>()) {
        DefVec fields{};

        // Strip return continuation from types, other lam values are not
        // supported anyway
        if (!pi->ret_pi()) return nullptr;

        for (auto field : pi->dom()->as<Sigma>()->projs().view().rsubspan(1))
            if (auto stripped = strip_rec(field)) fields.push_back(stripped);

        auto dom   = world.sigma(fields);
        auto codom = strip(pi->ret_pi()->dom());
        return world.pi(dom, codom, pi->is_implicit());
    }

    if (auto extract = def->isa<Extract>()) {
        if (Axm::isa<mem::M>(def->type())) {
            std::cerr << "  strip_rec: mem extract from " << def->unique_name()
                      << ", tuple=" << extract->tuple()->unique_name() << " (node: " << extract->tuple()->node_name()
                      << ", gid: " << extract->tuple()->gid() << ", type: " << extract->tuple()->type() << ")"
                      << std::endl;
            if (auto inner_extract = extract->tuple()->isa<Extract>())
                std::cerr << "    inner extract from: " << inner_extract->tuple()->unique_name()
                          << " (node: " << inner_extract->tuple()->node_name()
                          << ", type: " << inner_extract->tuple()->type() << ")" << std::endl;
            std::cerr << "    tuple in locals_: " << (locals_.count(extract->tuple()) ? "yes" : "no") << std::endl;
            emit(extract->tuple());
            std::cerr << "    after emit, tuple in locals_: " << (locals_.count(extract->tuple()) ? "yes" : "no")
                      << std::endl;
            return nullptr;
        }
        if (auto sigma = extract->tuple()->type()->isa<Sigma>()) {
            std::cerr << "  strip_rec: non-mem extract from " << def->unique_name()
                      << ", tuple=" << extract->tuple()->unique_name() << " (node: " << extract->tuple()->node_name()
                      << "), tuple in locals_: " << (locals_.count(extract->tuple()) ? "yes" : "no") << std::endl;
            size_t count = 0;
            auto index   = Lit::as(extract->index());
            for (auto stripped : sigma->projs([this](const Def* def) { return strip_rec(def); }))
                if (!stripped) {
                    if (count < index) index--;
                } else {
                    count++;
                }

            if (count > 1)
                return world.extract(extract->tuple(), index);
            else
                return strip_rec(extract->tuple());
        }
    }

    if (Axm::isa<mem::M>(def)) return nullptr;
    if (Axm::isa<spirv::Global>(def)) return nullptr;
    if (Axm::isa<spirv::entry>(def)) return nullptr;

    return def;
}

Word Emitter::convert(const Def* type, std::string_view name) {
    // check if already converted
    if (auto i = types_.find(type); i != types_.end()) return i->second;

    // create new id
    Word id      = next_id();
    id_names[id] = "ERROR";

    if (type == world().sigma()) {
        declarations.emplace_back(Op{OpKind::TypeVoid, {}, id, {}});
        id_names[id] = "void";
    } else if (type->isa<Nat>() || Idx::isa(type)) {
        // All integer types map to a single shared 32-bit unsigned integer type
        if (i32_type_id_.has_value()) {
            types_[type] = *i32_type_id_;
            return *i32_type_id_;
        }
        declarations.emplace_back(Op{
            OpKind::TypeInt,
            {32, 0},
            id,
            {}
        });
        id_names[id] = "i32";
        i32_type_id_ = id;
    } else if (auto w = math::isa_f(type)) {
        Word bitwidth = static_cast<Word>(*w);
        declarations.emplace_back(Op{
            OpKind::TypeFloat,
            {bitwidth, 0},
            id,
            {}
        });
        id_names[id] = std::format("f{}", bitwidth);
    } else if (auto arr = type->isa<Arr>()) {
        // Convert the element type
        Word elem_id = convert(arr->body());

        if (auto arity_lit = Lit::isa(arr->arity())) {
            u64 size = *arity_lit;

            // use vector for small arrays of scalars (2, 3, or 4 elements)
            // TODO: sizes 8 and 16 are also supported for some memory
            // models, add check
            if (size >= 2 && size <= 4 && is_scalar_type(arr->body())) {
                declarations.emplace_back(Op{
                    OpKind::TypeVector,
                    {elem_id, static_cast<Word>(size)},
                    id,
                    {}
                });
                id_names[id] = std::format("vec{}_{}", size, id_name(elem_id));
            } else {
                // always use i32 for arity
                Word length_id = emit(world().lit_idx(1ull << 32, size));

                declarations.emplace_back(Op{
                    OpKind::TypeArray,
                    {elem_id, length_id},
                    id,
                    {}
                });
                id_names[id] = std::format("arr{}_{}", size, id_name(elem_id));
            }
        } else {
            // TODO: runtime-sized arrays
            std::cerr << "dynamic arrays not yet supported\n";
            fe::unreachable();
        }
    } else if (auto pi = type->isa<Pi>()) {
        Word param_type  = convert(pi->dom());
        Word return_type = convert(pi->codom());

        // Do not emit void argument type
        std::vector<Word> ops{return_type};
        if (pi->dom() != world().sigma()) ops.push_back(param_type);

        Op op{OpKind::TypeFunction, ops, id, {}};
        declarations.push_back(op);
        id_names[id] = std::format("pi{}", type->unique_name());
    } else if (auto sigma = type->isa<Sigma>()) {
        if (sigma->isa_mut()) std::cerr << "mutable sigmas not yet supported\n";

        std::vector<Word> fields{};
        for (auto t : sigma->ops()) {
            if (Axm::isa<mem::M>(t)) continue;
            if (Axm::isa<spirv::entry>(t)) continue; // Skip entry markers
            if (Axm::isa<spirv::Global>(t)) {
                convert(t);
                continue;
            }
            fields.emplace_back(convert(t));
        }

        declarations.emplace_back(Op{OpKind::TypeStruct, fields, id, {}});
        id_names[id] = std::format("sigma{}", type->unique_name());
    }

    if (id_names[id] == "ERROR") std::cerr << "unsupported type: " << type << "\n";

    types_[type] = id;
    return id;
}

Word Emitter::emit_interface(std::string name, const Def* def) {
    if (interface_vars_.contains(def)) return interface_vars_[def];

    auto global                                        = Axm::as<spirv::Global>(def);
    auto [storage_class, n, decorations, wrapped_type] = global->uncurry_args<4>();
    auto _storage_class                                = Axm::as<spirv::storage>(storage_class);
    Word __storage_class;
    switch (_storage_class.id()) {
        case spirv::storage::INPUT: __storage_class = 1; break;
        case spirv::storage::UNIFORM: __storage_class = 2; break;
        case spirv::storage::OUTPUT: __storage_class = 3; break;
        default: fe::unreachable();
    }
    Word wrapped_type_id = convert(wrapped_type);

    // Check if pointer type already exists
    Word ptr_id;
    auto ptr_key = std::make_pair(__storage_class, wrapped_type_id);
    if (auto it = ptr_types_.find(ptr_key); it != ptr_types_.end()) {
        ptr_id = it->second;
    } else {
        ptr_id = next_id();

        // emit pointer type of var
        declarations.emplace_back(Op{
            OpKind::TypePointer,
            {__storage_class, wrapped_type_id},
            ptr_id,
            {}
        });
        id_names[ptr_id]    = std::format("ptr_{}_{}", id_name(wrapped_type_id), storage_class::name(__storage_class));
        ptr_types_[ptr_key] = ptr_id;
    }

    // store global interface variable in map
    // for access later
    Word interface_id    = next_id();
    interface_vars_[def] = interface_id;

    // emit var
    declarations.emplace_back(Op{OpKind::Variable, {__storage_class}, interface_id, ptr_id});
    id_names[interface_id] = std::format("{}", name);

    if (decorations->isa<Sigma>())
        for (auto decoration : decorations->ops())
            emit_decoration(interface_id, decoration);
    else
        emit_decoration(interface_id, decorations);

    return interface_id;
}

void Emitter::emit_decoration(Word var_id, const Def* decoration_) {
    auto decoration = Axm::as<spirv::decor>(decoration_);
    switch (decoration.id()) {
        case spirv::decor::builtin: {
            auto [_model, magic] = decoration->uncurry_args<2>();
            annotations.emplace_back(Op{
                OpKind::Decorate,
                {var_id, decoration::BuiltIn, static_cast<Word>(Lit::as(magic))},
                {},
                {},
            });
            break;
        }
        case spirv::decor::location:
            auto location = decoration->arg();
            annotations.emplace_back(Op{
                OpKind::Decorate,
                {var_id, decoration::Location, static_cast<Word>(Lit::as(location))},
                {},
                {},
            });
            break;
    }
}

std::optional<spirv::model> isa_builtin(const Def* type) {
    if (auto global = Axm::isa<spirv::Global>(type)) {
        auto [storage_class, n, decorations, wrapped_type] = global->uncurry_args<4>();
        for (auto decoration : decorations->ops()) {
            if (auto builtin = Axm::isa<spirv::decor>(decoration)) {
                if (builtin.id() == spirv::decor::builtin) {
                    auto model = Axm::as<spirv::model>(builtin->arg(0));
                    return std::optional<spirv::model>{model.id()};
                }
            }
        }
    }

    return std::nullopt;
}

Word Emitter::prepare() {
    Word id          = next_id();
    globals_[root()] = id;

    // Convert Pi type to direct style and strip
    const Pi* type      = strip(root()->type())->as<Pi>();
    Word type_id        = convert(type);
    Word return_type_id = convert(type->codom());
    funDefinitions.emplace_back(Op{
        OpKind::Function,
        {0, type_id},
        id,
        return_type_id
    });
    id_names[id] = root()->unique_name();

    // Handle function parameter
    auto var      = root()->var();
    auto var_type = type->dom();
    Word var_id   = next_id();
    if (var_type != world().sigma())
        funDefinitions.emplace_back(Op{OpKind::FunctionParameter, {}, var_id, convert(var_type)});
    id_names[var_id]                         = var->unique_name();
    locals_[world().extract(var, (size_t)0)] = var_id;

    // Handle entry point markers and interface variables
    std::optional<spirv::model> model{};
    std::vector<Word> interfaces{};
    std::vector<const Def*> exec_modes{};

    // A fun always has a sigma domain type consisting of some regular argument and the
    // return continuation, as long as the regular argument is not a continuation
    // of the same type, which is unsupported in the backend anyway, as Spir-V does
    // not support higher order programs.
    std::cerr << "dom: " << root()->dom() << " - " << root()->dom()->op(0)->node_name() << "\n";
    auto sigma = root()->dom()->op(0)->as<Sigma>();
    std::cerr << sigma << "\n";
    for (size_t idx = 0; idx < sigma->num_ops(); ++idx) {
        auto param = sigma->op(idx);

        // Check if this is an execution model marker
        if (auto entry_marker = Axm::isa<spirv::entry>(param)) {
            if (model.has_value()) error("multiple execution model markers found in entry point");

            // Extract model and modes: entry: Model → {n: Nat} → «n; Mode» → ★
            auto [_model, n, modes] = entry_marker->uncurry_args<3>();
            model                   = Axm::as<spirv::model>(_model).id();

            // Collect execution modes
            if (auto lit_n = Lit::isa(n)) {
                if (*lit_n == 1) {
                    // Single mode
                    exec_modes.push_back(modes);
                } else if (*lit_n > 1) {
                    // Multiple modes in a tuple
                    for (auto mode : modes->projs())
                        exec_modes.push_back(mode);
                }
                // n == 0: no modes
            }
            continue;
        }

        // Get interface name
        std::string interface_name = var->proj(0)->proj(idx)->unique_name();

        // Process global interface variables
        if (auto global = Axm::isa<spirv::Global>(param)) {
            auto ivar_id = emit_interface(interface_name, global);
            interfaces.push_back(ivar_id);

            // Validate that builtins align with the specified model
            auto builtin_model = isa_builtin(param);
            if (model.has_value() && builtin_model.has_value()) {
                if (*model != *builtin_model)
                    error("invalid builtin for specified execution model encountered in entry point");
            }

            // Add interface_var to locals_
            locals_[world().extract(world().extract(var, (size_t)0), idx)] = ivar_id;
            continue;
        }
    }

    // external lams are converted to entry points
    if (root()->is_external()) {
        // assume compute shader if no builtins were used
        if (!model.has_value()) model = spirv::model::compute;

        Word model_magic;
        switch (*model) {
            case spirv::model::vertex: model_magic = 0; break;
            case spirv::model::fragment: model_magic = 4; break;
            case spirv::model::compute: model_magic = 5; break;
        }

        Op entry{
            OpKind::EntryPoint,
            {model_magic, id},
            {},
            {}
        };

        // append name
        for (Word word : string_to_words(root()->sym().str()))
            entry.operands.push_back(word);

        // append interfacing globals
        for (Word word : interfaces)
            entry.operands.push_back(word);

        entryPoints.push_back(entry);

        // Emit execution modes
        for (auto mode_def : exec_modes) {
            if (auto mode = Axm::isa<spirv::mode>(mode_def)) {
                Word mode_magic;
                switch (mode.id()) {
                    case spirv::mode::invocations: mode_magic = execution_mode::Invocations; break;
                    case spirv::mode::spacing_equal: mode_magic = execution_mode::SpacingEqual; break;
                    case spirv::mode::spacing_fractional_even:
                        mode_magic = execution_mode::SpacingFractionalEven;
                        break;
                    case spirv::mode::spacing_fractional_odd: mode_magic = execution_mode::SpacingFractionalOdd; break;
                    case spirv::mode::vertex_order_cw: mode_magic = execution_mode::VertexOrderCw; break;
                    case spirv::mode::vertex_order_ccw: mode_magic = execution_mode::VertexOrderCcw; break;
                    case spirv::mode::pixel_center_integer: mode_magic = execution_mode::PixelCenterInteger; break;
                    case spirv::mode::origin_upper_left: mode_magic = execution_mode::OriginUpperLeft; break;
                    case spirv::mode::origin_lower_left: mode_magic = execution_mode::OriginLowerLeft; break;
                    case spirv::mode::early_fragment_tests: mode_magic = execution_mode::EarlyFragmentTests; break;
                    default: continue;
                }
                executionModes.push_back(Op{
                    OpKind::ExecutionMode,
                    {id, mode_magic},
                    {},
                    {}
                });
            }
        }
    }

    return id;
}

void Emitter::emit_epilogue(Lam* lam) {
    auto app = lam->body()->as<App>();
    auto& bb = lam2bb_[lam];

    Word lam_id      = id(lam);
    id_names[lam_id] = lam->unique_name();

    // For the root function's entry block, generate a unique label ID (different from function ID)
    // For other basic blocks, use their lam's ID
    if (lam == root()) {
        Word label_id      = next_id();
        id_names[label_id] = lam->unique_name() + "_entry";
        bb.label           = Op{OpKind::Label, {}, label_id, {}};
    } else {
        bb.label = Op{OpKind::Label, {}, id(lam), {}};
    }

    // emit bb end instruction
    if (app->callee() == root()->ret_var()) {
        // return lam called
        // => OpReturn | OpReturnValue

        std::vector<Word> values;
        std::vector<const Def*> types;

        for (auto arg : app->args()) {
            auto value    = emit(arg);
            auto arg_type = strip(arg->type());
            if (arg_type != world().sigma()) {
                values.emplace_back(value);
                types.emplace_back(arg_type);
            }
        }

        switch (values.size()) {
            case 0: bb.end = Op{OpKind::Return, {}, {}, {}}; break;
            case 1: bb.end = Op{OpKind::ReturnValue, {values[0]}, {}, {}}; break;
            default:
                Word val_id = next_id();
                Word type   = convert(world().sigma(types));
                bb.tail.emplace_back(Op{OpKind::CompositeConstruct, {values}, val_id, type});
        }
    } else if (auto dispatch = sflow::Dispatch(app)) {
        std::cerr << "dispatch handling for: " << app->unique_name() << std::endl;
        for (auto callee : dispatch.tuple()->projs([](const Def* def) { return def->isa_mut<Lam>(); })) {
            std::cerr << "  callee: " << callee->unique_name() << std::endl;
            size_t n = callee->num_tvars();
            for (size_t i = 0; i != n; ++i) {
                auto arg         = emit(dispatch.app()->arg(n, i));
                auto phi         = callee->var(n, i);
                auto extract_key = world().extract(callee->var(), i);
                std::cerr << "    adding to locals_: " << extract_key->unique_name() << " (phi: " << phi->unique_name()
                          << ")" << std::endl;
                assert(!Axm::isa<mem::M>(phi->type()));
                lam2bb_[callee].phis[phi].emplace_back(arg);
                lam2bb_[callee].phis[phi].emplace_back(id(lam));
                locals_[extract_key] = id(phi);
            }
        }

        // Emit structured control flow merge instructions if this is sflow.branch or sflow.loop
        if (dispatch.kind() == sflow::Dispatch::Kind::Branch) {
            bb.tail.emplace_back(Op{OpKind::SelectionMerge, {id(dispatch.merge())}, {}, {}});
        } else if (dispatch.kind() == sflow::Dispatch::Kind::Loop) {
            bb.tail.emplace_back(Op{
                OpKind::LoopMerge,
                {id(dispatch.merge()), id(dispatch.cont())},
                {},
                {}
            });
        }

        std::cerr << "dispatch.num_targets()=" << dispatch.num_targets() << std::endl;
        if (dispatch.num_targets() == 2) {
            // if cond { A args… } else { B args… }
            // => OpBranchConditional
            std::cerr << "BranchConditional: index=" << dispatch.index() << std::endl;
            std::cerr << "  tuple=" << dispatch.tuple() << std::endl;
            std::cerr << "  tuple deps:" << std::endl;
            for (auto dep : dispatch.tuple()->deps())
                std::cerr << "    " << dep << " (node: " << dep->node_name() << ")" << std::endl;
            Op op{OpKind::BranchConditional, {emit(dispatch.index())}, {}, {}};
            for (auto callee : dispatch.tuple()->ops())
                op.operands.push_back(id(callee));
            std::cerr << "  operands count: " << op.operands.size() << std::endl;
            bb.end = op;
        } else {
            // switch (index) { case 0: A args…; …; case n: Z args… }
            // => OpSwitch selector default_label [literal, label]*
            // Use last case as default since all cases are explicit in MimIR
            auto callees   = dispatch.tuple()->deps();
            auto num_cases = callees.size();
            assert(num_cases > 0);

            Op op{OpKind::Switch, {emit(dispatch.index())}, {}, {}};
            op.operands.push_back(id(callees[num_cases - 1])); // default = last case
            for (size_t i = 0; i + 1 < num_cases; ++i) {
                op.operands.push_back(int_to_words(i, 32)[0]);
                op.operands.push_back(id(callees[i]));
            }
            bb.end = op;
        }
    } else if (app->callee()->isa<Bot>()) {
        // unreachable
        // => OpUnreachable
        bb.end = Op{OpKind::Unreachable, {}, {}, {}};
    } else if (auto callee = Lam::isa_mut_basicblock(app->callee())) {
        // ordinary jump
        // => OpBranch
        std::cerr << "ordinary jump to: " << callee->unique_name() << std::endl;
        auto arg = emit(app->arg());
        auto phi = callee->var();
        lam2bb_[callee].phis[phi].emplace_back(arg);
        lam2bb_[callee].phis[phi].emplace_back(id(lam));
        locals_[callee->var()] = id(phi);
        bb.end                 = Op{OpKind::Branch, {id(callee)}, {}, {}};
    } else if (Pi::isa_returning(app->callee_type())) {
        // function call
        // => Op
    }
    // TODO: OpTerminateInvocation
}

Word Emitter::emit_bb(BB& bb, const Def* def) {
    std::cerr << "emit_bb: " << def->unique_name() << " (node: " << def->node_name() << "): " << def->type()
              << std::endl;

    auto original_def = def;
    def               = strip(def);
    if (!def) return -1;

    // Check if the stripped def was already emitted (e.g., via emit() in strip_rec)
    if (auto it = locals_.find(def); it != locals_.end()) return it->second;

    Word type_id = convert(strip(def->type()));

    Word id = next_id();

    // Cache the stripped def if it's different from the original
    // This ensures that when the same underlying value is accessed via different paths,
    // we don't emit it multiple times
    if (def != original_def) locals_[def] = id;
    id_names[id] = def->unique_name();

    if (auto tuple = def->isa<Tuple>()) {
        std::vector<Word> constituents;

        // Unit value
        if (tuple == world().tuple()) return id;

        // Emit all tuple elements
        for (size_t i = 0, n = tuple->num_projs(); i != n; ++i) {
            auto elem = tuple->proj(n, i);

            constituents.push_back(emit(elem));
        }

        // Directly unpack tuple if it only has a single or no values
        if (constituents.empty()) return emit(world().tuple());
        if (constituents.size() == 1) return constituents[0];

        if (is_const(tuple)) {
            // OpConstantComposite: result type is implicit, constituents are operands
            declarations.emplace_back(Op{OpKind::ConstantComposite, constituents, id, type_id});
        } else {
            // OpCompositeConstruct: result type ID first, then constituents
            constituents.insert(constituents.begin(), type_id);
            bb.ops.emplace_back(Op{OpKind::CompositeConstruct, constituents, id, type_id});
        }

        return id;
    }

    if (auto pack = def->isa<Pack>()) {
        auto arity = pack->arity();
        auto body  = pack->body();

        auto arity_val = 0;
        if (auto lit = Lit::isa(arity))
            arity_val = *lit;
        else
            error("arrays of dynamic size are not supported yet");

        Word body_id = emit(body);
        std::vector<Word> constituents;
        for (int i = 0; i < arity_val; i++)
            constituents.push_back(body_id);

        if (is_const(pack)) {
            // OpConstantComposite: result type is implicit, constituents are operands
            declarations.emplace_back(Op{OpKind::ConstantComposite, constituents, id, type_id});
        } else {
            // OpCompositeConstruct: result type ID first, then constituents
            constituents.insert(constituents.begin(), type_id);
            bb.ops.emplace_back(Op{OpKind::CompositeConstruct, constituents, id, type_id});
        }

        return id;
    }

    if (auto lit = def->isa<Lit>()) {
        if (lit->type()->isa<Nat>()) {
            // Nat: assume 32-bit for now
            // hints = {type=0 (int), width}
            declarations.push_back(Op{
                OpKind::Constant,
                int_to_words(lit->get(), 32),
                id,
                convert(lit->type()),
                {0, 32}
            });
            globals_[def] = id;
            return id;
        }

        if (Idx::isa(lit->type())) {
            declarations.push_back(Op{
                OpKind::Constant,
                int_to_words(lit->get(), 32),
                id,
                convert(lit->type()),
                {0, 32}
            });
            globals_[def] = id;
            return id;
        }

        if (auto w = math::isa_f(lit->type())) {
            // Float: hints = {type=1 (float), width}
            int width = static_cast<int>(*w);
            declarations.push_back(Op{
                OpKind::Constant,
                float_to_words(lit->get(), width),
                id,
                convert(lit->type()),
                {1, width}
            });
            globals_[def] = id;
            return id;
        }
    }

    if (auto cat = Axm::isa<vec::cat>(def)) {
        auto [nm, T, vs] = cat->uncurry_args<3>();
        auto [n, m]      = nm->projs<2>();
        auto [as, bs]    = vs->projs<2>();
        std::vector<Word> constituents;

        for (auto [n, vs] : {
                 std::pair{n, as},
                 std::pair{m, bs}
        }) {
            if (auto size = Lit::isa(n)) {
                if (size > 1) {
                    auto array = vs->type()->as<Arr>();
                    for (Word index = 0; index < size; index++) {
                        Word id      = next_id();
                        Word body_id = convert(array->body());
                        constituents.push_back(id);
                        bb.ops.push_back(Op{
                            OpKind::CompositeExtract,
                            {emit(vs), index},
                            id,
                            body_id,
                        });
                    }
                } else {
                    // size == 1, as size == 0 would be normalized away
                    constituents.push_back(emit(vs));
                }
            } else {
                error("runtime sized arrays not supported yet");
            }
        }

        bb.ops.push_back(Op{
            OpKind::CompositeConstruct,
            {constituents},
            id,
            type_id,
        });
        return id;
    }

    if (auto extract = def->isa<Extract>()) {
        auto tuple = extract->tuple();
        auto index = extract->index();

        std::cerr << "  extract from tuple: " << tuple->unique_name() << " (node: " << tuple->node_name() << ")"
                  << std::endl;
        std::cerr << "  index: " << index << std::endl;
        std::cerr << "  tuple is Var: " << (tuple->isa<Var>() ? "yes" : "no") << std::endl;
        if (tuple->isa<Var>())
            std::cerr << "  tuple in locals_: " << (locals_.count(tuple) ? "yes" : "no") << std::endl;

        // for literal indices, use OpCompositeExtract
        if (auto lit = Lit::isa(index)) {
            Word index = static_cast<Word>(*lit);

            bb.ops.push_back(Op{
                OpKind::CompositeExtract,
                {emit(tuple), index},
                id,
                type_id
            });

            return id;
        }

        // Dynamic indices require OpAccessChain
        // We need to:
        // 1. Create a pointer type for the composite
        // 2. Create a variable in Function storage with the composite as initializer
        // 3. Use OpAccessChain to get a pointer to the element
        // 4. Use OpLoad to load the value
        if (is_const(tuple)) {
            Word composite_id      = emit(tuple);
            Word composite_type_id = convert(tuple->type());

            // Get or create pointer type for the composite
            auto ptr_key = std::make_pair(static_cast<Word>(storage_class::Private), composite_type_id);
            Word ptr_type_id;
            if (auto it = ptr_types_.find(ptr_key); it != ptr_types_.end()) {
                ptr_type_id = it->second;
            } else {
                ptr_type_id = next_id();
                declarations.push_back(Op{
                    OpKind::TypePointer,
                    {storage_class::Private, composite_type_id},
                    ptr_type_id,
                    {}
                });
                id_names[ptr_type_id] = std::format("ptr_{}", id_name(composite_type_id));
                ptr_types_[ptr_key]   = ptr_type_id;
            }

            // Create variable with initializer
            Word var_id = next_id();
            declarations.push_back(Op{
                OpKind::Variable,
                {storage_class::Private, composite_id},
                var_id,
                ptr_type_id
            });

            // Get or create pointer type for the element
            auto elem_ptr_key = std::make_pair(static_cast<Word>(storage_class::Private), type_id);
            Word elem_ptr_type_id;
            if (auto it = ptr_types_.find(elem_ptr_key); it != ptr_types_.end()) {
                elem_ptr_type_id = it->second;
            } else {
                elem_ptr_type_id = next_id();
                declarations.push_back(Op{
                    OpKind::TypePointer,
                    {storage_class::Private, type_id},
                    elem_ptr_type_id,
                    {}
                });
                id_names[elem_ptr_type_id] = std::format("ptr_{}", id_name(type_id));
                ptr_types_[elem_ptr_key]   = elem_ptr_type_id;
            }

            // OpAccessChain to get pointer to element
            Word ptr_id = next_id();
            bb.ops.push_back(Op{
                OpKind::AccessChain,
                {var_id, emit(index)},
                ptr_id,
                elem_ptr_type_id
            });

            // OpLoad to load the value
            bb.ops.push_back(Op{OpKind::Load, {ptr_id}, id, type_id});

            return id;
        }
    }

    if (auto store = Axm::isa<spirv::store>(def)) {
        auto [mem, global, value] = store->arg()->projs<3>();
        emit(mem);
        bb.ops.emplace_back(Op{
            OpKind::Store,
            {emit(global), emit(value)},
            {},
            {}
        });
        return id;
    }

    if (auto load = Axm::isa<spirv::load>(def)) {
        auto [mem, global] = load->arg()->projs<2>();
        emit(mem);
        bb.ops.emplace_back(Op{
            OpKind::Load,
            {emit(global)},
            id,
            type_id,
        });
        return id;
    }

    if (auto bitcast = Axm::isa<core::bitcast>(def)) {
        auto src    = bitcast->arg();
        auto src_id = emit(src);

        auto size2width = [&](const Def* type) -> nat_t {
            if (type->isa<Nat>()) return 32;
            if (Idx::isa(type)) return 32;
            return 0;
        };

        auto src_width = size2width(src->type());
        auto dst_width = size2width(bitcast->type());

        if (src_width == dst_width) {
            // Same size: use OpBitcast or just return source
            if (convert(src->type()) == type_id) return src_id;
            bb.ops.push_back(Op{OpKind::Bitcast, {src_id}, id, type_id});
        } else if (src_width < dst_width) {
            // Widening: use OpUConvert (zero extend)
            bb.ops.push_back(Op{OpKind::UConvert, {src_id}, id, type_id});
        } else {
            // Narrowing: use OpUConvert (truncate)
            bb.ops.push_back(Op{OpKind::UConvert, {src_id}, id, type_id});
        }
        return id;
    }

    // Handle math comparisons - SPIR-V comparisons always return OpTypeBool
    if (auto cmp = Axm::isa<math::cmp>(def)) {
        auto [lhs, rhs]   = cmp->arg()->projs<2>();
        Word lhs_id       = emit(lhs);
        Word rhs_id       = emit(rhs);
        Word bool_type_id = bool_type();

        OpKind op_kind;
        switch (cmp.id()) {
            // Ordered comparisons (false if either is NaN)
            case math::cmp::e: op_kind = OpKind::FOrdEqual; break;
            case math::cmp::ne: op_kind = OpKind::FOrdNotEqual; break;
            case math::cmp::l: op_kind = OpKind::FOrdLessThan; break;
            case math::cmp::le: op_kind = OpKind::FOrdLessThanEqual; break;
            case math::cmp::g: op_kind = OpKind::FOrdGreaterThan; break;
            case math::cmp::ge: op_kind = OpKind::FOrdGreaterThanEqual; break;
            // Unordered comparisons (true if either is NaN)
            case math::cmp::ue: op_kind = OpKind::FUnordEqual; break;
            case math::cmp::une: op_kind = OpKind::FUnordNotEqual; break;
            case math::cmp::ul: op_kind = OpKind::FUnordLessThan; break;
            case math::cmp::ule: op_kind = OpKind::FUnordLessThanEqual; break;
            case math::cmp::ug: op_kind = OpKind::FUnordGreaterThan; break;
            case math::cmp::uge: op_kind = OpKind::FUnordGreaterThanEqual; break;
            // Special cases
            case math::cmp::f: // always false
                declarations.emplace_back(Op{OpKind::ConstantFalse, {}, id, bool_type_id});
                return id;
            case math::cmp::t: // always true
                declarations.emplace_back(Op{OpKind::ConstantTrue, {}, id, bool_type_id});
                return id;
            case math::cmp::o: // ordered (neither is NaN) - use FOrdEqual(x,x) && FOrdEqual(y,y)
            case math::cmp::u: // unordered (either is NaN) - use FUnordNotEqual(x,x) || FUnordNotEqual(y,y)
                std::cerr << "math.cmp.o and math.cmp.u not yet implemented\n";
                bb.ops.emplace_back(Op{OpKind::Undefined, {}, id, bool_type_id});
                return id;
            default:
                std::cerr << "unknown math.cmp variant\n";
                bb.ops.emplace_back(Op{OpKind::Undefined, {}, id, bool_type_id});
                return id;
        }

        bb.ops.emplace_back(Op{
            op_kind,
            {lhs_id, rhs_id},
            id,
            bool_type_id
        });
        return id;
    }

    if (def->isa<Var>()) {
        // Var nodes should have been registered in locals_ already (via phi handling)
        if (auto it = locals_.find(def); it != locals_.end()) return it->second;
        std::cerr << "Var not in locals_: " << def->unique_name() << "\n";
    }

    bb.ops.emplace_back(Op{OpKind::Undefined, {}, id, type_id});
    if (auto app = def->isa<App>())
        std::cerr << "def not yet implemented: " << app->callee() << ": " << def->type() << "\n";
    else
        std::cerr << "def not yet implemented: " << def << " (node: " << def->node_name() << "): " << def->type()
                  << "\n";

    return id;
}

void Emitter::finalize() {
    for (auto mut : Scheduler::schedule(nest())) {
        if (auto lam = mut->isa_mut<Lam>()) {
            assert(lam2bb_.contains(lam));
            auto& bb = lam2bb_[lam];

            // reserve space for ops
            funDefinitions.reserve(1 + bb.merge.has_value() + bb.phis.size() + bb.ops.size() + bb.tail.size() + 1);

            funDefinitions.push_back(bb.label);
            for (auto [phi, var_parents] : bb.phis)
                funDefinitions.emplace_back(Op{OpKind::Phi, var_parents, id(phi), convert(phi->type())});
            funDefinitions.insert(funDefinitions.end(), bb.ops.begin(), bb.ops.end());
            funDefinitions.insert(funDefinitions.end(), bb.tail.begin(), bb.tail.end());
            if (bb.merge) funDefinitions.push_back(*bb.merge);
            funDefinitions.push_back(bb.end);
        }
    }

    funDefinitions.emplace_back(Op{OpKind::FunctionEnd, {}, {}, {}});
}

void emit_bin(World& world, std::ostream& ostream) {
    Emitter emitter(world, ostream);
    emitter.run();
    emitter.binary();
}

void emit_asm(World& world, std::ostream& ostream) {
    Emitter emitter(world, ostream);
    emitter.run();
    emitter.assembly();
}
} // namespace mim::plug::spirv
