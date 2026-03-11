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
        for (auto [flags, annex] : old_world().flags2annex())
            sym2flags_[annex->sym().str()] = flags;

        sym2type_["top"]  = new_world().type_top();
        sym2type_["bot"]  = new_world().type_bot();
        sym2type_["bool"] = new_world().type_bool();
        sym2type_["nat"]  = new_world().type_nat();
        sym2type_["i1"]   = new_world().type_i1();
        sym2type_["i2"]   = new_world().type_i2();
        sym2type_["i4"]   = new_world().type_i4();
        sym2type_["i8"]   = new_world().type_i8();
        sym2type_["i16"]  = new_world().type_i16();
        sym2type_["i32"]  = new_world().type_i32();
        sym2type_["i64"]  = new_world().type_i64();
    }

    void process(RewriteResult rewrite);

    void init(MimNode node);
    void init_var(MimNode node);
    void init_lam(MimNode node);
    void init_con(MimNode node);

    void convert(MimNode node, bool recurse = false);
    void convert_lam(MimNode node);
    void convert_con(MimNode node);
    void convert_app(MimNode node);
    void convert_var(MimNode node);
    void convert_lit(MimNode node);
    void convert_arr(MimNode node);
    void convert_tuple(MimNode node);
    void convert_extract(MimNode node);
    void convert_ins(MimNode node);
    void convert_sigma(MimNode node);
    void convert_cn(MimNode node);
    void convert_pi(MimNode node);
    void convert_idx(MimNode node);
    void convert_hole(MimNode node);
    void convert_type(MimNode node);
    void convert_num(MimNode node);
    void convert_symbol(MimNode node);

    MimNode set_curr(int id) {
        curr_id_ = id;
        return res_[curr_id_];
    }

    void add_def(const Def* converted) { added_[curr_id_] = converted; }
    void add_var(std::string name, const Def* converted) { vars_[name] = converted; }

    const Def* get_def(int id) { return added_.contains(id) ? added_[id] : nullptr; }
    const Def* get_var(std::string name) { return vars_[name]; }

    MimNode get_node(MimKind expected, int id) {
        assert(res_[id].kind == expected && "get_node: mismatch between expected and actual node kind");
        return res_[id];
    }
    std::string get_symbol(int id) { return res_[id].symbol.c_str(); }
    int get_num(int id) { return res_[id].num; }

    int curr_id_;
    rust::Vec<MimNode> res_;
    std::unordered_map<int, const Def*> added_;
    std::unordered_map<std::string, const Def*> vars_;

    std::unordered_map<std::string, flags_t> sym2flags_;
    std::unordered_map<std::string, const Def*> sym2type_;
};

}; // namespace mim::plug::eqsat
