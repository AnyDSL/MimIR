#include "thorin/be/llvm/cuda.h"

#include <fstream>
#include <stdexcept>

#include "thorin/primop.h"
#include "thorin/world.h"
#include "thorin/be/c.h"

namespace thorin {

CUDACodeGen::CUDACodeGen(World& world)
    : CodeGen(world, llvm::CallingConv::C, llvm::CallingConv::C, llvm::CallingConv::C)
{}

void CUDACodeGen::emit(bool debug) {
    auto name = get_output_name(world_.name());
    std::ofstream file(name);
    if (!file.is_open())
        throw std::runtime_error("cannot write '" + name + "': " + strerror(errno));
    thorin::emit_c(world_, file, Lang::CUDA, debug);
}

}
