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

#include "mim/plug/mem/mem.h"
#include "mim/plug/spirv/autogen.h"
#include "mim/plug/spirv/be/op.h"
#include "mim/plug/vec/autogen.h"

#include "absl/container/flat_hash_map.h"
#include "fe/assert.h"

using namespace std::string_literals;

namespace mim::plug::spirv {

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

/// Returns a unit type if the type is not a real type in
/// the spirv target. This applies currently to
/// - mem::M
/// - spirv::Global
/// - spirv::entry
const Def* isa_kept(const Def* type) {
    auto& world = type->world();

    if (Axm::isa<mem::M>(type)) return world.sigma();
    if (Axm::isa<spirv::Global>(type)) return world.sigma();
    if (Axm::isa<spirv::entry>(type)) return world.sigma();
    return type;
}

/// Recursively removes any types that should not
/// be emitted as actual types to spirv.
const Def* strip_type(const Def* type) {
    auto& world = type->world();

    if (auto sigma = type->isa<Sigma>()) {
        DefVec fields{};
        for (auto field : sigma->ops()) {
            auto stripped = strip_type(field);
            if (stripped != world.sigma()) fields.push_back(stripped);
        }

        if (fields.empty()) return world.sigma();
        if (fields.size() == 1) return fields[0];

        return world.sigma(fields.view());
    }

    return isa_kept(type);
}

template<typename Iter>
std::string words_to_string(Iter& begin, Iter end) {
    std::string out;
    Word mask = (1 << 8) - 1;
    for (; begin != end; begin++) {
        Word word = *begin;
        for (int index = 0; index < 4; index++)
            if (auto c = (word >> (index * 8)) & mask)
                out.push_back(static_cast<char>(c));
            else {
                // Advance past the null-terminator word
                ++begin;
                return out;
            }
    }
    return out;
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
        id_names[glsl_ext_inst_id_] = "GLSL.std.450";

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
    // Converts a type into a spirv type referenced by the
    // returned id.
    Word convert(const Def* type, std::string_view name = "");
    Word convert_ret_pi(const Pi* type);
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
            case OpKind::Function:
                ostream() << " " << function_control::name(op.operands[0]);
                ostream() << " %" << id_name(op.operands[1]);
                break;
            case OpKind::Decorate:
                ostream() << " %" << id_name(op.operands[0]);
                ostream() << " " << decoration::name(op.operands[1]);
                if (op.operands[1] == decoration::BuiltIn && op.operands.size() > 2) {
                    ostream() << " " << builtin::name(op.operands[2]);
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
                ostream() << " " << (op.operands[1] ? "Signed" : "Unsigned");
                break;
            case OpKind::TypeFloat:
                ostream() << " " << op.operands[0]; // width
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
    DefMap<Word> interface_vars_{};
};

Word Emitter::convert(const Def* type, std::string_view name) {
    std::cerr << "converting: " << type << "\n";

    // check if already converted
    if (auto i = types_.find(type); i != types_.end()) return i->second;

    // create new id
    Word id      = next_id();
    id_names[id] = "ERROR";

    if (type == world().sigma()) {
        declarations.emplace_back(Op{OpKind::TypeVoid, {}, id, {}});
        id_names[id] = "void";
    } else if (type->isa<Nat>()) {
        declarations.emplace_back(Op{
            OpKind::TypeInt,
            {64, 0},
            id,
            {}
        });
        id_names[id] = "nat";
    } else if (auto size = Idx::isa(type)) {
        Word bitwidth = Idx::size2bitwidth(size).value_or(64);
        declarations.emplace_back(Op{
            OpKind::TypeInt,
            {64, 0},
            id,
            {}
        });
        id_names[id] = std::format("i{}", bitwidth);
    } else if (auto w = math::isa_f(type)) {
        Word bitwidth = static_cast<Word>(*w);
        std::cerr << "float with width " << bitwidth << "\n";
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
        assert(Pi::isa_returning(pi) && "should never have to convert type of BB");
        Word return_type = convert_ret_pi(pi->ret_pi());
        Op op{OpKind::TypeFunction, {return_type}, id, {}};

        auto doms = strip_type(pi->dom());
        std::cerr << "doms: " << doms << "\n";
        if (doms != world().sigma()) {
            // if it is not a sigma, it is just the continuation, which
            // we don't want to convert
            if (auto sigma = doms->isa<Sigma>())
                for (auto dom : sigma->ops().rsubspan(1)) {
                    if (Axm::isa<mem::M>(dom)) continue;
                    if (Axm::isa<spirv::entry>(dom)) continue; // Skip entry markers
                    std::cerr << "dom: " << dom << "\n";
                    op.operands.emplace_back(convert(dom));
                }
        }

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

        // TODO: don't catch all apps
    } else if (auto app = type->isa<App>()) {
        if (auto global = Axm::isa<spirv::Global>(app)) {
            auto [storage_class, n, decorations, wrapped_type] = app->uncurry_args<4>();
            std::cerr << "storage class: " << storage_class << "\n";
            std::cerr << "n: " << n << "\n";
            std::cerr << "decorations: " << decorations << "\n";
            std::cerr << "wrapped type: " << wrapped_type << "\n";
            if (auto _storage_class = Axm::isa<spirv::storage>(storage_class)) {
                Word __storage_class;
                switch (_storage_class.id()) {
                    case spirv::storage::INPUT: __storage_class = 1; break;
                    case spirv::storage::UNIFORM: __storage_class = 2; break;
                    case spirv::storage::OUTPUT: __storage_class = 3; break;
                    default: fe::unreachable();
                }
                Word _wrapped_type = convert(wrapped_type);

                // emit pointer type of var
                declarations.emplace_back(Op{
                    OpKind::TypePointer,
                    {__storage_class, _wrapped_type},
                    id,
                    {}
                });
                // TODO: better name (add storage class to it)
                id_names[id] = std::format("ptr_{}_{}", id_name(_wrapped_type), storage_class::name(__storage_class));

                // store global interface variable in map
                // for access later
                Word var_id           = next_id();
                interface_vars_[type] = var_id;
                std::cerr << "Stored interface_vars_[" << type << "] = " << var_id << "\n";

                // emit var
                declarations.emplace_back(Op{OpKind::Variable, {__storage_class}, var_id, id});
                // Use provided name if available, otherwise fall back to type name
                if (!name.empty())
                    id_names[var_id] = std::format("{}", name);
                else
                    id_names[var_id]
                        = std::format("{}_{}", storage_class::name(__storage_class), id_name(_wrapped_type));

                // TODO: emit decorations
                if (auto sigma = decorations->isa<Sigma>())
                    for (auto decoration : decorations->ops())
                        emit_decoration(var_id, decoration);
                else
                    emit_decoration(var_id, decorations);
            }
        }
    }

    if (id_names[id] == "ERROR") std::cerr << "unsupported type: " << type << "\n";

    types_[type] = id;
    return id;
}

Word Emitter::convert_ret_pi(const Pi* pi) {
    auto dom = strip_type(pi->dom());
    std::cerr << "ret pi dom: " << dom << "\n";
    return convert(dom);
}

void Emitter::emit_decoration(Word var_id, const Def* decoration_) {
    std::cerr << "decoration: " << decoration_ << "\n";
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

    std::cerr << "preparing " << root()->unique_name() << "\n";

    Word type        = convert(root()->type());
    Word return_type = convert_ret_pi(root()->type()->ret_pi());
    funDefinitions.emplace_back(Op{
        OpKind::Function,
        {0, type},
        id,
        return_type
    });
    id_names[id] = root()->unique_name();

    std::optional<spirv::model> model{};
    std::vector<Word> interfaces{};

    auto var = root()->var(0);

    if (Axm::isa<mem::M>(var->type())) {
        // do nothing
    } else if (auto sigma = var->type()->isa<Sigma>()) {
        for (size_t idx = 0; idx < sigma->num_ops(); ++idx) {
            auto param = sigma->op(idx);

            if (Axm::isa<mem::M>(param)) continue;

            // Check if this is an execution model marker
            if (auto entry_marker = Axm::isa<spirv::entry>(param)) {
                if (model.has_value()) error("multiple execution model markers found in entry point");

                // Extract the model from the entry marker argument
                model = Axm::as<spirv::model>(entry_marker->arg()).id();
                continue;
            }

            // Extract parameter name from projection
            std::string param_name;
            try {
                auto proj = var->proj(sigma->num_ops(), idx);
                if (proj) param_name = proj->unique_name();
            } catch (...) {
                // If projection fails, use empty name
            }

            // Process global interface variables
            if (auto global = Axm::isa<spirv::Global>(param)) {
                convert(param, param_name);
                auto var_id = interface_vars_[param];
                interfaces.push_back(var_id);

                // Validate that builtins align with the specified model
                auto builtin_model = isa_builtin(param);
                if (model.has_value() && builtin_model.has_value()) {
                    if (*model != *builtin_model)
                        error("invalid builtin for specified execution model encountered in entry point");
                }

                // Add var extract to locals_, as it is not real
                locals_[world().extract(var, idx)] = var_id;
                continue;
            }

            auto param_type = strip_type(var->type());

            // Handle "real" parameters
            if (param_type != world().sigma()) {
                Word param_id = next_id();

                funDefinitions.emplace_back(Op{OpKind::FunctionParameter, {}, param_id, convert(param_type)});
                id_names[param_id] = param->unique_name();

                // Add var extract to locals_, as it is not real
                locals_[world().extract(var, idx)] = param_id;
            }
        }
    } else {
        auto param_type = strip_type(var->type());
        if (param_type != world().sigma()) {
            Word param_id = next_id();

            funDefinitions.emplace_back(Op{OpKind::FunctionParameter, {}, param_id, convert(param_type)});
            id_names[param_id] = var->unique_name();
            locals_[var]       = param_id;
        }
    }

    // external lams are converted to entry points
    if (root()->is_external()) {
        // assume compute shader if no builtins were used
        if (!model.has_value()) model = spirv::model::Compute;

        Word model_magic;
        switch (*model) {
            case spirv::model::Vertex: model_magic = 0; break;
            case spirv::model::Fragment: model_magic = 4; break;
            case spirv::model::Compute: model_magic = 5; break;
        }

        Op entry{
            OpKind::EntryPoint,
            {model_magic, id},
            {},
            {}
        };

        // append name
        for (Word word : string_to_words(root()->unique_name()))
            entry.operands.push_back(word);

        // append interfacing globals
        for (Word word : interfaces) {
            entry.operands.push_back(word);
            std::cerr << "Appending interface var " << word << " (name: " << id_name(word) << ")\n";
        }

        std::cerr << "interfaces: " << interfaces.size() << "\n";
        std::cerr << "Entry point operands size: " << entry.operands.size() << "\n";

        entryPoints.push_back(entry);
    }

    return id;
}

void Emitter::emit_epilogue(Lam* lam) {
    auto app = lam->body()->as<App>();
    auto& bb = lam2bb_[lam];

    Word lam_id      = id(lam);
    id_names[lam_id] = lam->unique_name();

    bb.label = Op{OpKind::Label, {}, id(lam), {}};

    // emit bb end instruction
    if (app->callee() == root()->ret_var()) {
        // return lam called
        // => OpReturn | OpReturnValue

        // generate new id for first basic block in function
        bb.label = Op{OpKind::Label, {}, next_id(), {}};

        std::vector<Word> values;
        std::vector<const Def*> types;

        for (auto arg : app->args()) {
            auto value    = emit(arg);
            auto arg_type = strip_type(arg->type());
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
    } else if (auto dispatch = Dispatch(app)) {
        // TODO
        std::cerr << "branching not supported yet";
        if (auto branch = Branch(app)) {
            // if cond { A args… } else { B args… }
            // => OpBranchConditional
        } else {
            // switch (index) { case 0: A args…; …; default: B args… }
            // => OpSwitch
        }
    } else if (app->callee()->isa<Bot>()) {
        // unreachable
        // => OpUnreachable
        bb.end = Op{OpKind::Unreachable, {}, {}, {}};
    } else if (auto callee = Lam::isa_mut_basicblock(app->callee())) {
        // ordinary jump
        // => OpBranch
        size_t n = callee->num_tvars();
        for (size_t i = 0; i != n; ++i) {
            auto arg = emit(app->arg(n, i));
            auto phi = callee->var(n, i);
            assert(!Axm::isa<mem::M>(phi->type()));
            lam2bb_[callee].phis[phi].emplace_back(arg);
            lam2bb_[callee].phis[phi].emplace_back(id(lam));
            locals_[phi] = id(phi);
        }
        bb.end = Op{OpKind::Branch, {id(lam)}, {}, {}};
    } else if (Pi::isa_returning(app->callee_type())) {
        // function call
        // => Op
    }
    // TODO: OpTerminateInvocation
}

Word Emitter::emit_bb(BB& bb, const Def* def) {
    OpVec ops{};

    Word id      = next_id();
    Word type_id = convert(def->type());

    if (auto tuple = def->isa<Tuple>()) {
        Word type_id = convert(tuple->type());
        std::vector<Word> constituents;

        // Emit all tuple elements
        for (size_t i = 0, n = tuple->num_projs(); i != n; ++i) {
            auto elem = tuple->proj(n, i);
            constituents.push_back(emit(elem));
        }

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

    if (auto lit = def->isa<Lit>()) {
        // TODO
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
            std::cerr << "vs: " << as << "\n";

            if (auto size = Lit::isa(n)) {
                if (size > 1) {
                    auto index = 0;
                    for (auto v : vs->type()->ops()) {
                        Word id = next_id();
                        constituents.push_back(id);
                        bb.ops.push_back(Op{
                            OpKind::CompositeExtract,
                            {emit(vs), index},
                            id,
                            convert(v),
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

        // mem values should not happen here
        assert(!Axm::isa<mem::M>(extract->type()) && "mem value extraction encountered");
        // var extracts are not real and should have been added to locals_ already
        assert(!def->isa<Var>() && "var extractions encountered in emit_bb");

        // For non-parameter extracts, emit OpCompositeExtract
        // TODO: implement OpCompositeExtract for actual composite extractions
        return id;
    }

    if (auto store = Axm::isa<spirv::store>(def)) {
        auto [mem, global, value] = store->arg()->projs<3>();
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
        bb.ops.emplace_back(Op{
            OpKind::Load,
            {emit(global)},
            id,
            type_id,
        });
        return id;
    }

    // TODO: vars

    bb.ops.emplace_back(Op{OpKind::Error, {}, id, type_id});
    if (auto app = def->isa<App>())
        std::cerr << "def not yet implemented: " << app->callee() << "\n";
    else
        std::cerr << "def not yet implemented: " << def << "\n";

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
