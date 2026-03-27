from ctypes import cdll
import os
import ctypes
import subprocess
import sys
from pathlib import Path
from typing import Self
from regex_plug import regex
from core_plug import *
# Add our build dir to the python modules list
build_dir = os.path.abspath("../build/lib/")
sys.path.insert(0, build_dir)

import mim  # Import from that list

# this is currently very rudimentary, actual test cases will follow
# mim_h_plugin_path = os.fspath()
# ---------------------------------------------------------------------
"""
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
"""

driver = mim.Driver()
driver.log().set_stdout().set(mim.Level.Debug)
driver.add_search_path(Path("../build/lib/mim/"))

# world  = driver.world()

from typing import List

"""
the current call fn does not handle self.wrld.call(f"%core.bit2.and_", [self.wrld.lit_nat_0(), [matched, eq_zero]])

my attempt to fix:

def call(self):





"""

# def call(self, callee, *args, by_id=False) -> mim.Def:
#     # print(f'{callee.str()} is the callee')
#     if isinstance(callee, str):
#         print(f"callee is string, received string: {callee}")
#         callee = self.sym(callee)
#     callee = self.annex(callee)

#     # if(len(args) == 0 and not by_id):
#     #     return callee
#     # elif(len(args) == 0 and by_id):
#     #     pass
#     if len(args) == 0:
#         return callee

#     if len(args) == 1:
#         if isinstance(args[0], mim.Def):
#             return self.implicit_app(callee, [args[0]])
#         # check if the passed arguments are def or a list of def
#         # next we need to discern if its a mix of defs and def all passed as the arg, or just one of the two
#         elif isinstance(args[0], list):
#             # if the list only contains single elements (its a flat array) then we just pass it to the implicit app call
#             if(all(isinstance(x, mim.Def) for x in args[0])):
#                 return self.implicit_app(callee, args[0])
#             # otherwise we have a nested array and need to handle
#             else:
#                 return self.implicit_app(callee, args[0])

#         else:
#             print(f"received: {callee} and len args is 1")
#             raise TypeError("The given arguments dont match the expected types")

#     if isinstance(args[0], list) and all(isinstance(x, mim.Def) for x in args[0]):
#         return self.call(self.implicit_app(callee, args[0]), args[1::])

#     if isinstance(args[0], mim.Def):
#         return self.call(self.implicit_app(callee, [args[0]]), args[1::])

#     print(f"received: {callee}")
#     raise TypeError("The given arguments dont match the expected types")
# ----------------------------------------------------------


def call(self, *args) -> mim.Def:
    
    callee = args[0]


    if isinstance(args[0], regex):
        callee = self.annex_by_id(args[0].value)
    if isinstance(callee, str):
        callee = self.sym(callee)
        callee = self.annex(callee)
    if isinstance(callee, mim.Sym):
        callee = self.annex(callee)
        if len(args) == 1:
            return callee

    if len(args) == 1:
        return args[0]

    if len(args) >= 3:
        if isinstance(args[1], List):
            return self.call(self.implicit_app(callee, args[1]), args[2::])
        else:
            return self.call(self.implicit_app(callee, [args[1]]), args[2::])
    else:
        if isinstance(args[1], List):
            return self.implicit_app(callee, args[1])
        else:
            # print(f"first argument: {type(args[1])}")
            if isinstance(args[1], tuple):
                tmp = list(args[1])
                return self.implicit_app(callee, tmp[0])
            return self.implicit_app(callee, [args[1]])
    raise TypeError("The given arguments dont match the expected types")


mim.World.call = call


class MimRegex:
    def __init__(self, driver: mim.Driver, *args):
        self.wrld = driver.world()
        self.regex = []
        driver.load_pluins(["core", "compile", "regex", "opt"])

        self._star_sym = self.wrld.sym(f"%regex.quant.star")
        self._optional_sym = self.wrld.sym(f"%regex.quant.optional")
        self._plus_sym = self.wrld.sym(f"%regex.quant.plus")
        self._literal_sym = self.wrld.sym(f"%regex.lit")
        self._any_sym = self.wrld.sym(f"%regex.any")
        self._conj_sym = self.wrld.sym(f"%regex.conj")

    def __char_lit(self, lit) -> mim.Def:
        return self.wrld.lit_i8(ord(lit))

    def star(self) -> Self:
        # self.__conj(self.regex)
        # self.regex.append(self.wrld.call_by_id(int(0x4c62066400000901), [self.regex[0]]))
        self.regex.append(self.wrld.call(self._star_sym, [self.regex[len(self.regex)-1]]))
        return self

    # def dot(self) -> Self:
    #     self.regex.append(self.wrld.call(self.any_sym))
    #     return Self

    def literal(self, lit) -> Self:
        # print("printing")
        # print(self.__char_lit(lit))
        # self.regex.append(self.wrld.call_by_id(int(0x4c62066400000300), [self.__char_lit(lit)]))
        self.regex.append(self.wrld.call(regex.lit, self.__char_lit(lit)))
        return self

    def close(self) -> Self:
        self.__conj(self.regex)
        return self

    def __conj(self, expr: list) -> Self:
        self.regex = []
        self.regex.append(self.wrld.call(regex.conj,expr))
        return self

    def any(self) -> Self:
        self.regex.append(self.wrld.call(self._any_sym))
        return self

    def build(self, driver):
        # print(f"result of annexing with call: {self.wrld.call('%mem.M')}")
        self.match_func = self.wrld.mut_con(
            [
                self.wrld.call("%mem.M", self.wrld.lit_nat_0()),
                self.wrld.call(
                    "%mem.Ptr0",
                    [self.wrld.arr(self.wrld.top_nat(), self.wrld.type_i8())],
                ),
                self.wrld.cn(
                    [
                        self.wrld.call("%mem.M", self.wrld.lit_nat_0()),
                        self.wrld.type_bool(),
                    ]
                ),
            ]
        ).set("match_func")

        self.match_func.externalize()
        mem, to_match, exit = self.match_func.var().projs(3)
        
        # this is an error any user could make, calling a function on a def which results in a segfault
        # mem.var()
        #     ^^^^^ this is not allowed, there needs to be error tracing for these cases
        print(self.literal("a"))
        regex_mem, matched, pos = self.wrld.implicit_app(
           self.regex[0],
            [mem, to_match, self.wrld.lit(self.wrld.type_idx(self.wrld.top_nat()), 0)],
        ).projs(3)
        last_elem_ptr = self.wrld.call("%mem.lea", [to_match, pos])
        (final_mem, last_elem) = self.wrld.call(
            "%mem.load", [regex_mem, last_elem_ptr]
        ).projs(2)
        eq_zero = self.wrld.call_by_id(
            core.icmp.xyglEe,[last_elem, self.wrld.lit_i8(0)]
        )
        """
        nested arrays dont work rn, need to pass py::obj to the call_ fn and then iteratively destructure it to pass the items to the next call site
        """
        matched_and_end = self.wrld.call(
            f"%core.bit2.and_", self.wrld.lit_nat_0(), [matched, eq_zero]
        )
        self.match_func.app(False, exit, [final_mem, matched_and_end])
        self.wrld.optimize()
        driver.backend("ll", "regex.ll", self.wrld)
        subprocess.run(["clang", "regex.ll", "-o", "regex.so", "-Wno-override-module", "-shared"], check=True)
        self.exec()
        
    def exec(self):
        so = cdll.LoadLibrary("./regex.so")
        so.match_func.argtypes = [ctypes.c_char_p]
        so.match_func.restype = ctypes.c_bool
        to_match  = "abcdabcdabcd"
        dont_match = "cbaa"
        to_match = to_match.encode("utf-8")
        dont_match = dont_match.encode("utf-8")
        if(so.match_func(to_match)):
            print("expression matches the regex")
        if(not so.match_func(dont_match)):
            print("expression does not match the regex")
        


reg = MimRegex(driver)
# seg fault
# reg.any().close()
reg.literal("a").literal("b").literal("c").literal("d").close().star().close()
print(reg.regex)
reg.build(driver)
reg.wrld.dot("out_no_literals", True, False)
import platform
print(platform.platform())
