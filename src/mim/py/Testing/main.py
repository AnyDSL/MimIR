import sys
import os
import subprocess
from pathlib import Path
# Add our build dir to the python modules list
build_dir = os.path.abspath("../../../../build/lib/")
sys.path.insert(0, build_dir)

import mim  # Import from that list
# this is currently very rudimentary, actual test cases will follow
# mim_h_plugin_path = os.fspath()

driver = mim.Driver()
driver.log().set_stdout().set(mim.Level.Debug)
driver.add_search_path(Path("../../../../build/lib/mim/"))

world  = driver.world()
# py_world = mim.PyWorld(world)

mem = world.sym(f"%mem.M")
ptr = world.sym(f"%mem.Ptr0")
#world.sym(f"%compile")
print(ptr)

#####
ast = mim.AST(world)
parser = mim.PyParser(mim.Parser(ast))
#parser = mim.Parser(ast)
parser.plugin("core")
parser.plugin("compile")
mem_t = world.annex(mem)
#following line of code is just to observe mim_error behaviour
mem_t = world.call(mem, [world.call(mem, [world.type_i32()])])
argv_t = world.call(ptr, [world.call(ptr, [world.type_i32()])])
main = world.mut_fun2([mem_t, world.type_i32(), argv_t], [mem_t, world.type_i32()]).set("main")

print(main.var().num_projs())

args, ret = main.var().projs(2)
mem, argc, argv = args.projs(3)
main.app(False, ret, [mem, argc])
main.externalize()
#world.optimize()

driver.backend("ll", "hello.ll", world)

subprocess.run(["clang", "hello.ll", "-o", "hello", "-Wno-override-module"])




print(mem, argc, argv)
print(type(main))

print("mem_t: ")
print(mem_t)
print("-----------")
print("arg_v: ")
print(argv_t)
print("-----------")

#####

# ast = mim.AST(driver.world())
# parser = mim.PyParser(mim.Parser(ast))
# print(mem, ptr)
# parser.plugin("core")
# i32_type = py_world.type_i32()
# # argv_t = py_world.implicit_app(i32_type)
# ann = py_world.type_i32()
# arg_v = py_world.annex(ptr)
# mem_t = py_world.annex(mem)
# print(type(py_world))
# main = py_world.mut_fun(mem_t, [mem_t, arg_v])
# main.externalize()
# mem = main.var().proj(1)
# print("---------------------")
# print(mem)

# print("---------------------")
# print(mem_t)
# # print(sym)
# # print(mem_t)
# print(ann)
# print(ast)
# print(mem)
# #assert(False, test_str.empty())
# print(mem)
# #print(f"is sym empty?: {sym.empty()}")
# #assert(True, sym.empty())
# #print(driver)
# py_world.write()
