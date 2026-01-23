import sys
import os
import subprocess
from pathlib import Path
from typing import Self
import ctypes
# Add our build dir to the python modules list
build_dir = os.path.abspath("../../../../build/lib/")
sys.path.insert(0, build_dir)

import mim  # Import from that list
# this is currently very rudimentary, actual test cases will follow
# mim_h_plugin_path = os.fspath()
# ---------------------------------------------------------------------
'''
muss das für jeden regex neu compiled werden?

load the .dll and have a pointer to the function

std::function<bool(const char*)> MimirCodeGen::make_matcher(MimRegex re) {
    mim_match(re);

    mim::optimize(world_);

    auto tmp = std::filesystem::temp_directory_path();

    auto shared_lib = mim::fmt("{}/regex-{}.{}", tmp.string(), std::this_thread::get_id(), mim::dl::extension);

    if (compile_to_shared(shared_lib) == 0)
        world_.DLOG("Compiled regex to shared library: {}", shared_lib);
    else
        throw std::runtime_error{"0: error: Failed to compile regex to shared library."};

    // dl::open throws on error
    jit_lib_.reset(mim::dl::open(shared_lib.c_str()));

    std::function<bool(const char*)> fn_handle = (bool (*)(const char*))mim::dl::get(jit_lib_.get(), MATCHER_FUNC_NAME);
    return fn_handle;
}
'''

driver = mim.Driver()
driver.log().set_stdout().set(mim.Level.Debug)
driver.add_search_path(Path("../../../../build/lib/mim/"))

# world  = driver.world()


class MimRegex():

    def __init__(self, driver):
        self.wrld = driver.world()
        self.regex = []
        driver.load_pluins(["core", "compile", "regex"])

        self._star_sym = self.wrld.sym(f"%regex.quant.star")
        self._optional_sym = self.wrld.sym(f"%regex.quant.optional")
        self._plus_sym = self.wrld.sym(f"%regex.quant.plus")
        self._literal_sym = self.wrld.sym(f"%regex.lit")
        self._any_sym = self.wrld.sym(f"%regex.any")
        self._conj_sym = self.wrld.sym(f"%regex.conj")

    def __char_lit(self, lit) -> mim.Def:
        return self.wrld.lit_i8(ord(lit))

    def star(self) -> Self:
        self.regex.append(self.wrld.call_by_id(int(0x4c62066400000901), self.regex[len(self.regex)-1]))
        return self

    # def dot(self) -> Self:
    #     self.regex.append(self.wrld.call(self.any_sym))
    #     return Self

    def literal(self, lit) -> Self:
        # print("printing")
        # print(self.__char_lit(lit))
        self.regex.append(self.wrld.call_by_id(int(0x4c62066400000300), [self.__char_lit(lit)]))
        return self
    
    def close(self) -> Self:
        self.__conj(self.regex)
        return self

    def __conj(self, expr: list) -> Self:
        self.regex = []
        self.regex.append(self.wrld.call(self._conj_sym, expr))
        return self

    def any(self) -> Self:
        self.regex.append(self.wrld.call(self._any_sym))
        return self


    def build(self, driver):
        self.match_func = self.wrld.mut_con(
            [
                self.wrld.call("%mem.M"),
                self.wrld.call("%mem.Ptr0", [self.wrld.arr(self.wrld.top_nat(), self.wrld.type_i8())]),
                self.wrld.cn([self.wrld.call("%mem.M"), self.wrld.type_bool()])
            ]
        ).set("match_func")

        self.match_func.externalize()
        mem, to_match, exit = self.match_func.var().projs(3)
        # this is an error any user could make, calling a function on a def which results in a segfault
        # mem.var()
        #     ^^^^^ this is not allowed, there needs to be error tracing for these cases

        regex_mem, matched, pos = self.wrld.implicit_app(
            self.regex[0], [mem, to_match, self.wrld.lit(self.wrld.type_idx(self.wrld.top_nat()), 0)]
        ).projs(3) 
        last_elem_ptr = self.wrld.call("%mem.lea", [to_match, pos])
        (final_mem, last_elem) = self.wrld.call("%mem.load", [regex_mem, last_elem_ptr]).projs(2)
        eq_zero = self.wrld.call_by_id(int(0x1104c60000000b01),[last_elem, self.wrld.lit_i8(0)])
        '''
        nested arrays dont work rn, need to pass py::obj to the call_ fn and then iteratively destructure it to pass the items to the next call site
        '''
        matched_and_end = self.wrld.call(f"%core.bit2._and", [self.wrld.lit_nat_0(), [matched, eq_zero]])
        self.match_func.app(False, exit, [final_mem, matched_and_end])


        # driver.backend("ll", "regex.ll", self.wrld)

        # subprocess.run(["clang", "regex.ll", "-o", "regex.so", "-Wno-override-module", "-shared"])

        # ctypes.cdll("./regex.so")

reg = MimRegex(driver)
# seg fault
#reg.any().close()
reg.literal("a").literal("b").literal("c").literal("d").star()
reg.build(driver)
#world.dot("out_no_literals", True, False)