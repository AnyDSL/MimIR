#include "mim/plug/spirv/be/emit.h"

#include <ostream>
#include <string>

#include <mim/plug/math/math.h>

#include "mim/def.h"
#include "mim/tuple.h"

#include "mim/be/emitter.h"

#include "mim/plug/mem/mem.h"
#include "mim/plug/spirv/autogen.h"
#include "mim/plug/spirv/be/op.h"

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

using OpVec = std::vector<Op>;

struct BB {
    OpVec ops;
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
    Word convert(const Def* type);
    Word convert_ret_pi(const Pi* type);

    // Returns a new unique id. To make ids as low as possible, as
    // recommended by the spirv spec, they are counted up instead of
    // reusing MimIR ids.
    Word next_id() { return next_id_++; }

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
        switch (op.kind) {
            case OpKind::Capability: ostream() << " " << capability::name(op.operands[0]); break;
            case OpKind::ExtInstImport: ostream() << " " << ext_inst::name(op.operands[0]); break;
            case OpKind::MemoryModel:
                ostream() << " " << addressing_model::name(op.operands[0]);
                ostream() << " " << memory_model::name(op.operands[1]);
                break;
            case OpKind::Function:
                ostream() << " %" << id_name(op.operands[0]);
                ostream() << " " << function_control::name(op.operands[1]);
                ostream() << " %" << id_name(op.operands[2]);
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

    absl::flat_hash_map<Word, std::string> id_names;

    Word next_id_{0};
    Word glsl_ext_inst_id_{0};
};

Word Emitter::convert(const Def* type) {
    // check if already converted
    if (auto i = types_.find(type); i != types_.end()) return i->second;

    // create new id
    Word id = next_id();

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
    } else if (auto bitwidth_l = math::isa_f(type)) {
        Word bitwidth = static_cast<Word>(bitwidth_l.value_or(64));
        declarations.emplace_back(Op{
            OpKind::TypeFloat,
            {bitwidth, 0},
            id,
            {}
        });
        switch (bitwidth) {
            case 16: id_names[id] = "f16"; break;
            case 32: id_names[id] = "f32"; break;
            case 64: id_names[id] = "f64"; break;
            default: std::cerr << "weird float size\n"; fe::unreachable();
        }
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
        Op type{OpKind::TypeFunction, {return_type}, {}, {}};

        auto doms = pi->doms();
        for (auto dom : doms.view().rsubspan(1)) {
            if (Axm::isa<mem::M>(dom)) continue;
            type.operands.emplace_back(convert(dom));
        }

        declarations.push_back(type);
    } else if (auto sigma = type->isa<Sigma>()) {
        if (sigma->isa_mut()) std::cerr << "mutable sigmas not yet supported\n";

        std::vector<Word> fields{};
        for (auto t : sigma->ops())
            fields.emplace_back(convert(t));

        declarations.emplace_back(Op{OpKind::TypeStruct, fields, id, {}});
    } else {
        std::cerr << "type not yet implemented: " << type->node_name() << "\n";
        // fe::unreachable();
    }

    types_[type] = id;
    return id;
}

Word Emitter::convert_ret_pi(const Pi* pi) {
    auto dom = mem::strip_mem_ty(pi->dom());
    return convert(dom);
}

Word Emitter::prepare() {
    Word id          = next_id();
    globals_[root()] = id;

    Word type        = convert(root()->type());
    Word return_type = convert_ret_pi(root()->type()->ret_pi());
    funDefinitions.emplace_back(Op{
        OpKind::Function,
        {return_type, 0, type},
        id,
        return_type
    });

    auto vars = root()->vars();
    for (auto var : vars.view().rsubspan(1)) {
        if (Axm::isa<mem::M>(var->type())) continue;

        Word param_id   = next_id();
        Word param_type = convert(var->type());

        funDefinitions.emplace_back(Op{OpKind::FunctionParameter, {}, param_id, param_type});

        id_names[param_id] = var->unique_name();
        locals_[var]       = param_id;
    }

    return id;
}

void Emitter::emit_epilogue(Lam* lam) {
    auto app = lam->body()->as<App>();
    auto& bb = lam2bb_[lam];

    if (app->callee() == root()->ret_var()) { // return
        // if (auto wrap = Axm::isa<spirv::wrap>(app->arg())) {
        //     Op entry_point{OpKind::EntryPoint, {}, {}, {}};
        //     auto [model, nm, dnm, ins, outs, in_ts, out_ts, f] = wrap->uncurry_args<8>();
        //     if (auto model_ = Axm::isa<spirv::model>(model)) {
        //         switch (model_.id()) {
        //             case spirv::model::fragment: entry_point.operands.emplace_back(execution_model::Fragment); break;
        //             case spirv::model::vertex: entry_point.operands.emplace_back(execution_model::Vertex); break;
        //             case spirv::model::compute: entry_point.operands.emplace_back(execution_model::GLCompute); break;
        //         }
        //     }

        //     entryPoints.push_back(entry_point);
        //     return;
        // }
    }
}

Word Emitter::emit_bb(BB& bb, const Def* def) {
    OpVec ops{};

    Word id = next_id();

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
            id_names[id] = std::format("const_composite_{}", id);
        } else {
            // OpCompositeConstruct: result type ID first, then constituents
            constituents.insert(constituents.begin(), type_id);
            bb.ops.emplace_back(Op{OpKind::CompositeConstruct, constituents, id, type_id});
            id_names[id] = std::format("composite_{}", id);
        }

        return id;
    }

    std::cerr << "def not yet implemented: " << def->node_name() << "\n";

    return id;
}

void Emitter::finalize() { funDefinitions.emplace_back(Op{OpKind::FunctionEnd, {}, {}, {}}); }

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
