#pragma once

#include <mim/phase.h>

#include "mim/def.h"
#include "mim/rewrite.h"

#include "egg/cxxbridge/eqsat/src/lib.rs.h"

namespace mim::plug::eqsat {

class EggRewrite : public Phase, public Rewriter {
public:
    EggRewrite(World& world, std::string name)
        : Phase(world, std::move(name))
        , Rewriter(world.inherit()) {
        register_symbols();
    }
    EggRewrite(World& world, flags_t annex)
        : Phase(world, annex)
        , Rewriter(world.inherit()) {
        register_symbols();
    }

    void start() override;

    using Phase::world;
    using Rewriter::world;
    World& world() = delete;
    World& old_world() { return Phase::world(); }
    World& new_world() { return Rewriter::world(); }

private:
    void register_symbols() {
        for (auto [flags, annex] : old_world().flags2annex()) {
            sym2flags_[annex->sym().str()] = flags;
            new_world().register_annex(flags, rewrite(annex));
        }

        sym2def_["Univ"] = new_world().univ();
        sym2def_["Bool"] = new_world().type_bool();
        sym2def_["Nat"]  = new_world().type_nat();
        sym2def_["I8"]   = new_world().type_i8();
        sym2def_["I16"]  = new_world().type_i16();
        sym2def_["I32"]  = new_world().type_i32();
        sym2def_["I64"]  = new_world().type_i64();
        sym2def_["ff"]   = new_world().lit_ff();
        sym2def_["tt"]   = new_world().lit_tt();
        sym2def_["i8"]   = new_world().lit_nat(0x100);
        sym2def_["i16"]  = new_world().lit_nat(0x10000);
        sym2def_["i32"]  = new_world().lit_nat(0x100000000);
    }

    rust::Vec<RuleSet> import_rulesets();

    const Def* init(uint32_t id, bool lambdas = false, bool bindings = false);
    const Def* init_lam(uint32_t id, MimNode node);
    const Def* init_con(uint32_t id, MimNode node);
    const Def* init_let(uint32_t id, MimNode node);

    const Def* convert(uint32_t id, bool recurse = false);
    const Def* convert_let(uint32_t id, MimNode node);
    const Def* convert_lam(uint32_t id, MimNode node);
    const Def* convert_con(uint32_t id, MimNode node);
    const Def* convert_app(uint32_t id, MimNode node);
    const Def* convert_var(uint32_t id, MimNode node);
    const Def* convert_lit(uint32_t id, MimNode node);
    const Def* convert_pack(uint32_t id, MimNode node);
    const Def* convert_tuple(uint32_t id, MimNode node);
    const Def* convert_extract(uint32_t id, MimNode node);
    const Def* convert_ins(uint32_t id, MimNode node);
    const Def* convert_bot(uint32_t id, MimNode node);
    const Def* convert_top(uint32_t id, MimNode node);
    const Def* convert_arr(uint32_t id, MimNode node);
    const Def* convert_sigma(uint32_t id, MimNode node);
    const Def* convert_cn(uint32_t id, MimNode node);
    const Def* convert_pi(uint32_t id, MimNode node);
    const Def* convert_idx(uint32_t id, MimNode node);
    const Def* convert_hole(uint32_t id, MimNode node);
    const Def* convert_type(uint32_t id, MimNode node);
    const Def* convert_num(uint32_t id, MimNode node);
    const Def* convert_symbol(uint32_t id, MimNode node);

    // A node that is associated with a Def can be:
    // 1) A node representing an arbitrary term
    // 2) A symbol node representing an annex
    // 3) A symbol node representing a type or term alias
    // 4) A symbol node representing a variable
    // 5) A symbol node representing a lambda
    const Def* get_def(uint32_t id) {
        auto def = added_[id];
        if (def == nullptr) {
            auto sym = get_symbol(id);
            if (sym2def_.contains(sym))
                def = sym2def_[sym];
            else if (sym2flags_.contains(sym))
                def = new_world().annex(sym2flags_[sym]);
            else if (vars_.contains(sym))
                def = get_var(sym);
            else if (lams_.contains(sym))
                def = get_lam(sym);
        }
        return def;
    }

    void register_var(std::string name, const Def* converted) { vars_[name] = converted; }
    void register_lam(std::string name, const Lam* converted) { lams_[name] = converted; }
    const Def* get_var(std::string name) { return vars_[name]; }
    const Lam* get_lam(std::string name) { return lams_[name]; }

    MimNode get_node(MimKind expected, uint32_t id) {
        assert(res_[id].kind == expected && "get_node: mismatch between expected and actual node kind");
        return res_[id];
    }
    MimNode get_node_unsafe(uint32_t id) { return res_[id]; }
    std::string get_symbol(uint32_t id) { return res_[id].symbol.c_str(); }
    int64_t get_num(uint32_t id) { return res_[id].num; }

    std::string remove_uid(std::string name) {
        if (auto pos = name.rfind("_"); pos != std::string::npos) {
            auto maybe_uid = name.substr(pos + 1);
            if (!maybe_uid.empty() && std::all_of(maybe_uid.begin(), maybe_uid.end(), ::isdigit))
                return name.substr(0, name.rfind("_"));
        }
        return name;
    }

    rust::Vec<MimNode> res_;
    std::unordered_map<uint32_t, const Def*> added_;
    // TODO: use actual driver.sym() symbols instead of strings
    std::unordered_map<std::string, const Def*> vars_;
    std::unordered_map<std::string, const Lam*> lams_;
    std::unordered_map<std::string, flags_t> sym2flags_;
    std::unordered_map<std::string, const Def*> sym2def_;
};

}; // namespace mim::plug::eqsat
