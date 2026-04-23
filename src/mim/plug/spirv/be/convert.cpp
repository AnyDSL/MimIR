#include "mim/def.h"

#include "mim/plug/math/math.h"
#include "mim/plug/mem/autogen.h"
#include "mim/plug/spirv/be/emit.h"

namespace mim::plug::spirv {

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
            emit_term(extract->tuple());
            return nullptr;
        }
        if (auto sigma = extract->tuple()->type()->isa<Sigma>()) {
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
    // if (Axm::isa<spirv::Global>(def)) return nullptr;
    // if (Axm::isa<spirv::entry>(def)) return nullptr;

    return def;
}

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

Word Emitter::emit_type(const Def* type) {
    // check if already converted
    if (auto i = types_.find(type); i != types_.end()) return i->second;

    // create new id
    Word id              = next_id();
    module_.id_names[id] = "ERROR";

    if (type == world().sigma()) {
        module_.declarations.emplace_back(Op{OpKind::TypeVoid, {}, id, {}});
        module_.id_names[id] = "void";
    } else if (Idx::isa(type)) {
        // All index types map to a single shared 32-bit unsigned integer type
        if (i32_type_id_.has_value()) {
            types_[type] = *i32_type_id_;
            return *i32_type_id_;
        }
        module_.declarations.emplace_back(Op{
            OpKind::TypeInt,
            {32, 0},
            id,
            {}
        });
        module_.id_names[id] = "u32";
        i32_type_id_         = id;
    } else if (type->isa<Nat>() || Idx::isa(type)) {
        module_.declarations.emplace_back(Op{
            OpKind::TypeInt,
            {32, 1},
            id,
            {}
        });
        module_.id_names[id] = "i32";
        i32_type_id_         = id;
    } else if (auto w = math::isa_f(type)) {
        Word bitwidth = static_cast<Word>(*w);
        module_.declarations.emplace_back(Op{
            OpKind::TypeFloat,
            {bitwidth, 0},
            id,
            {}
        });
        module_.id_names[id] = std::format("f{}", bitwidth);
    } else if (auto arr = type->isa<Arr>()) {
        // Convert the element type
        Word elem_id = emit_type(arr->body());

        if (auto arity_lit = Lit::isa(arr->arity())) {
            u64 size = *arity_lit;

            // use vector for small arrays of scalars (2, 3, or 4 elements)
            // TODO: sizes 8 and 16 are also supported for some memory
            // models, add check
            if (size >= 2 && size <= 4 && is_scalar_type(arr->body())) {
                module_.declarations.emplace_back(Op{
                    OpKind::TypeVector,
                    {elem_id, static_cast<Word>(size)},
                    id,
                    {}
                });
                module_.id_names[id] = std::format("vec{}_{}", size, id_name(elem_id));
            } else {
                // always use i32 for arity
                Word length_id = emit_term(world().lit_idx(1ull << 32, size));

                module_.declarations.emplace_back(Op{
                    OpKind::TypeArray,
                    {elem_id, length_id},
                    id,
                    {}
                });
                module_.id_names[id] = std::format("arr{}_{}", size, id_name(elem_id));
            }
        } else {
            // TODO: runtime-sized arrays
            std::cerr << "dynamic arrays not yet supported\n";
            fe::unreachable();
        }
    } else if (auto pi = type->isa<Pi>()) {
        Word param_type  = emit_type(pi->dom());
        Word return_type = emit_type(pi->codom());

        // Do not emit void argument type
        std::vector<Word> ops{return_type};
        if (pi->dom() != world().sigma()) ops.push_back(param_type);

        Op op{OpKind::TypeFunction, ops, id, {}};
        module_.declarations.push_back(op);
        module_.id_names[id] = std::format("pi{}", type->unique_name());
    } else if (auto sigma = type->isa<Sigma>()) {
        if (sigma->isa_mut()) std::cerr << "mutable sigmas not yet supported\n";

        std::vector<Word> fields{};
        for (auto t : sigma->ops())
            fields.emplace_back(emit_type(t));

        module_.declarations.emplace_back(Op{OpKind::TypeStruct, fields, id, {}});
        module_.id_names[id] = std::format("sigma{}", type->unique_name());
    }

    if (module_.id_names[id] == "ERROR") std::cerr << "unsupported type: " << type << "\n";

    types_[type] = id;
    return id;
}

} // namespace mim::plug::spirv
