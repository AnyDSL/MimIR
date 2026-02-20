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

private:
    rust::Vec<MimNode> res_;
    DefVec added_;
    absl::node_hash_map<std::string, const Def*> sym_table_;
};

}; // namespace mim::plug::eqsat
