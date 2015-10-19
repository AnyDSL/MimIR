#include "thorin/be/llvm/opencl.h"

#include <fstream>
#include <stdexcept>

#include "thorin/primop.h"
#include "thorin/world.h"
#include "thorin/be/c.h"


namespace thorin {

OpenCLCodeGen::OpenCLCodeGen(World& world)
    : CodeGen(world, llvm::CallingConv::C, llvm::CallingConv::C, llvm::CallingConv::C)
{}

void OpenCLCodeGen::emit(bool debug) {
    auto name = get_output_name(world_.name());
    std::ofstream file(name);
    if (!file.is_open())
        throw std::runtime_error("cannot write '" + name + "': " + strerror(errno));
    thorin::emit_c(world_, file, Lang::OPENCL, debug);
}

}
