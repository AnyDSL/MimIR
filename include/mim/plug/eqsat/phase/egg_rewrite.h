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
        , Rewriter(world.inherit()) {}
    EggRewrite(World& world, flags_t annex)
        : Phase(world, annex)
        , Rewriter(world.inherit()) {}

    void start() override;

    using Phase::world;
    using Rewriter::world;
    World& world() = delete;
    World& old_world() { return Phase::world(); }
    World& new_world() { return Rewriter::world(); }

private:
    void convert_lam(MimNode node);
    void convert_con(MimNode node);
    void convert_app(MimNode node);
    void convert_var(MimNode node);
    void convert_lit(MimNode node);
    void convert_tuple(MimNode node);
    void convert_extract(MimNode node);
    void convert_ins(MimNode node);

    void convert_sigma(MimNode node);
    void convert_arr(MimNode node);
    void convert_cn(MimNode node);
    void convert_idx(MimNode node);

    void convert_num(MimNode node);
    void convert_symbol(MimNode node);

    void add_def(const Def* converted) { added_[curr_id_] = converted; }
    void add_symbol(rust::String sym, const Def* converted) { sym_table_[sym.c_str()] = converted; }

    MimNode get_node(MimKind expected, int id) {
        assert(res_[id].kind == expected && "get_node: mismatch between expected and actual node kind");
        return res_[id];
    }
    const Def* get_def(int id) { return added_.contains(id) ? added_[id] : nullptr; }
    rust::String get_symbol(int id) { return res_[id].symbol; }
    int get_num(int id) { return res_[id].num; }

    int curr_id_;
    rust::Vec<MimNode> res_;
    std::unordered_map<int, const Def*> added_;
    std::unordered_map<std::string, const Def*> sym_table_;
};

}; // namespace mim::plug::eqsat
