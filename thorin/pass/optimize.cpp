#include "thorin/pass/optimize.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/rw/auto_diff.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/alloc2malloc.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/remem_elim.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

// old stuff
// #include "thorin/transform/cleanup_world.h"
// #include "thorin/transform/partial_evaluation.h"
// #include "thorin/transform/mangle.h"


#include "thorin/error.h"

namespace thorin {


void graph_print(std::ofstream& ofs, DefSet& done, const Def* def, int maxDepth);

void optimize(World& world) {
    PassMan::run<Scalerize>(world, nullptr);
    PassMan::run<EtaRed>(world);
    PassMan::run<TailRecElim>(world, nullptr);
    printf("Getting started\n");

    // world.set(LogLevel::Debug);
    // world.dbg(LogLevel::Debug);
    // world.set(std::make_unique<ErrorHandler>());

    world.set_log_level(LogLevel::Debug);

    PassMan pre_auto_opt(world);
    pre_auto_opt.add<PartialEval>();
    pre_auto_opt.run();


//    std::unique_ptr<ErrorHandler> err;
//    ErrorHandler* err;
//    world.set((std::unique_ptr<ErrorHandler>&&) nullptr);

    PassMan optA(world);
    optA.add<AutoDiff>();

//     PassMan optZ(world);
//     optZ.add<ZipEval>();
//     optZ.run();
//     printf("Finished OptiZip\n");

//     return;


//     PassMan opt2(world);
//     auto br = opt2.add<BetaRed>();
//     auto er = opt2.add<EtaRed>();
//     auto ee = opt2.add<EtaExp>(er);
//     opt2.add<SSAConstr>(ee);
//     opt2.add<Scalerize>(ee);
//     // opt2.add<DCE>(br, ee);
//     opt2.add<CopyProp>(br, ee);
//     opt2.add<TailRecElim>(er);
// //    opt2.run();
    printf("Finished Prepare Opti\n");

    optA.run();
    printf("Finished AutoDiff Opti\n");


    PassMan opt(world);
    opt.add<PartialEval>();
    auto br = opt.add<BetaRed>();
    auto er = opt.add<EtaRed>();
    auto ee = opt.add<EtaExp>(er);
    opt.add<SSAConstr>(ee);
    opt.add<Scalerize>(ee);
    opt.add<CopyProp>(br, ee);
    opt.add<TailRecElim>(er);
    opt.run();

    // PassMan opt3(world);
    // opt3.add<PartialEval>();
    // auto br3 = opt3.add<BetaRed>();
    // auto er3 = opt3.add<EtaRed>();
    // auto ee3 = opt3.add<EtaExp>(er);
    // opt3.add<SSAConstr>(ee3);
    // opt3.add<Scalerize>(ee3);
    // // opt3.add<DCE>(br3, ee3);
    // opt3.add<CopyProp>(br3, ee3);
    // opt3.add<TailRecElim>(er3);
    // opt3.run();
    printf("Finished Simpl Opti\n");




//         cleanup_world(world);
// //     partial_evaluation(world, true);
//     while (partial_evaluation(world, true)) {} // lower2cff
//         cleanup_world(world);

    printf("Finished Cleanup\n");
    PassMan::run<LamSpec>(world);

    PassMan codgen_prep(world);
    // codgen_prep.add<BoundElim>();
    codgen_prep.add<RememElim>();
    codgen_prep.add<Alloc2Malloc>();
    codgen_prep.add<RetWrap>();
    codgen_prep.run();

    // create a file graph.dot
//    std::ofstream ofs("graph.dot");
//    ofs << "digraph G {\n";


//    DefSet done;
//    for (const auto& [_, nom] : world.externals())
//        graph_print(ofs,done, nom, 4000);
//    ofs << "}\n";
//    ofs.close();
}


// void graph_print(std::ofstream& ofs, DefSet& done, const Def* def, int maxDepth) {
//     if (maxDepth < 0) return;
//     if (!done.emplace(def).second) return;

// //    do_sth(def);

//     u32 id = def->gid();
// //    const char *content=def->to_string().c_str();

//     ofs << "  " << id << " [label=\"" << def->to_string().c_str() << "\"];\n";
//     printf("%d: %s\n", def->gid(), def->to_string().c_str());

//     for (auto op : def->ops()) {
// //        for (auto op : def->extended_ops()) {
//         u32 op_id = op->gid();
//         ofs << "  " << id << " -> " << op_id << ";\n";
//         graph_print(ofs,done, op, maxDepth-1);
//     }
// }



} // namespace thorin
