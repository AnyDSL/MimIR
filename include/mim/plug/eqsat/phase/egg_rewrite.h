#pragma once

#include <mim/phase.h>

#include "mim/def.h"

#include "../src/mim/plug/eqsat/egg/target/cxxbridge/eqsat/src/lib.rs.h"

namespace mim::plug::eqsat {

class EggRewrite : public RWPhase {
public:
    EggRewrite(World& world, flags_t annex)
        : RWPhase(world, annex) {}

    void start() override;

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

    void add_def(MimNode node, const Def* converted) { added_[node] = converted; }
    void add_symbol(rust::String sym, const Def* converted) { sym_table_[sym] = converted; }

    // TODO: what should be returned in case something isn't found?
    // - i.e. get_num and get_symbol return 0 and "" respectively when
    // the node is not a Num or Symbol node
    // - get_def would panic though if called with a node that hasn't been
    // put into added_ yet
    MimNode get_node(int id) { return res_[id]; }
    const Def* get_def(int id) { return added_[res_[id]]; }
    rust::String get_symbol(int id) { return res_[id].symbol; }
    int get_num(int id) { return res_[id].num; }

    rust::Vec<MimNode> res_;
    std::unordered_map<MimNode, const Def*> added_;
    absl::node_hash_map<std::string, const Def*> sym_table_;
};

}; // namespace mim::plug::eqsat
