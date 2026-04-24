#pragma once

#include <mim/phase.h>

#include "mim/def.h"
#include "mim/rewrite.h"

#include "rust/mimir_eqsat.h"

namespace mim::plug::eqsat {

class SlottedRewrite : public Phase, public Rewriter {
public:
    SlottedRewrite(World& world, std::string name)
        : Phase(world, std::move(name))
        , Rewriter(world.inherit()) {
        register_symbols();
    }
    SlottedRewrite(World& world, flags_t annex)
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
            auto new_annex                = new_world().register_annex(flags, rewrite(annex));
            axms_[new_annex->sym().str()] = new_annex;
        }

        aliases_["Univ"] = new_world().univ();
        aliases_["Bool"] = new_world().type_bool();
        aliases_["Nat"]  = new_world().type_nat();
        aliases_["I8"]   = new_world().type_i8();
        aliases_["I16"]  = new_world().type_i16();
        aliases_["I32"]  = new_world().type_i32();
        aliases_["I64"]  = new_world().type_i64();
        aliases_["ff"]   = new_world().lit_ff();
        aliases_["tt"]   = new_world().lit_tt();
        aliases_["i8"]   = new_world().lit_nat(0x100);
        aliases_["i16"]  = new_world().lit_nat(0x10000);
        aliases_["i32"]  = new_world().lit_nat(0x100000000);
    }

    std::pair<rust::Vec<RuleSet>, CostFn> import_config();

    enum InitStage {
        Declarations,
        Lambdas,
        Bindings,
    };
    void init(rust::Vec<RewriteResult> rewrites, InitStage stage);
    const Def* init_lam(uint32_t id, MimNode node);
    const Def* init_con(uint32_t id, MimNode node);
    const Def* init_let(uint32_t id, MimNode node);
    const Def* init_axm(uint32_t id, MimNode node);

    void convert(rust::Vec<RewriteResult> rewrites);
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
    const Def* convert_insert(uint32_t id, MimNode node);
    const Def* convert_inj(uint32_t id, MimNode node);
    const Def* convert_merge(uint32_t id, MimNode node);
    const Def* convert_match(uint32_t id, MimNode node);
    const Def* convert_proxy(uint32_t id, MimNode node);
    const Def* convert_join(uint32_t id, MimNode node);
    const Def* convert_meet(uint32_t id, MimNode node);
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
            if (aliases_.contains(sym))
                def = aliases_[sym];
            else if (axms_.contains(sym))
                def = get_axm(sym);
            else if (vars_.contains(sym))
                def = get_var(sym);
            else if (lams_.contains(sym))
                def = get_lam(sym);
        }
        return def;
    }

    void register_var(std::string name, const Def* converted) { vars_[name] = converted; }
    void register_axm(std::string name, const Axm* converted) { axms_[name] = converted; }
    void register_lam(std::string name, const Lam* converted) { lams_[name] = converted; }
    void register_projs(uint32_t id) {
        auto node = get_node_unsafe(id);
        if (node.kind == MimKind::Var) {
            auto var      = get_def(node.children[0]);
            auto var_type = get_node_unsafe(node.children.back());
            if (var && (var_type.kind == MimKind::Sigma || var_type.kind == MimKind::Arr)) {
                size_t proj_idx = 0;
                for (size_t i = 1; i < node.children.size() - 1; i++) {
                    auto proj_node       = get_node(MimKind::Var, node.children[i]);
                    auto proj_name       = get_symbol(proj_node.children[0]);
                    auto proj_name_nouid = remove_uid(proj_name);
                    auto proj            = var->proj(proj_idx++);
                    proj->set(proj_name_nouid);
                    register_var(proj_name, proj);
                }
            }
        }
        for (uint32_t child : node.children)
            register_projs(child);
    }

    const Def* get_var(std::string name) { return vars_[name]; }
    const Def* get_axm(std::string name) { return axms_[name]; }
    const Lam* get_lam(std::string name) { return lams_[name]; }

    MimNode get_node(MimKind expected, uint32_t id) {
        assert(res_[id].kind == expected && "get_node: mismatch between expected and actual node kind");
        return res_[id];
    }
    MimNode get_node_unsafe(uint32_t id) { return res_[id]; }
    std::string get_symbol(uint32_t id) { return res_[id].symbol.c_str(); }
    uint64_t get_num(uint32_t id) { return res_[id].num; }

    std::string remove_uid(std::string name) {
        if (auto pos = name.rfind("_"); pos != std::string::npos) {
            auto maybe_uid = name.substr(pos + 1);
            if (!maybe_uid.empty() && std::all_of(maybe_uid.begin(), maybe_uid.end(), ::isdigit))
                return name.substr(0, pos);
        }
        return name;
    }

    rust::Vec<MimNode> res_;
    std::unordered_map<uint32_t, const Def*> added_;
    std::unordered_map<std::string, const Def*> vars_;
    std::unordered_map<std::string, const Lam*> lams_;
    std::unordered_map<std::string, const Def*> axms_;
    std::unordered_map<std::string, const Def*> aliases_;
};

}; // namespace mim::plug::eqsat
