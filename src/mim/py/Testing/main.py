import sys
import os
from pathlib import Path
# Add our build dir to the python modules list
build_dir = os.path.abspath("../../../../build/lib/")
sys.path.insert(0, build_dir)

import mim  # Import from that list
# this is currently very rudimentary, actual test cases will follow
#mim_h_plugin_path = os.fspath()

driver = mim.Driver()
driver.log().set_stdout().set(mim.Level.Debug)
driver.add_search_path(Path("../../../../build/lib/mim/"))
world  = driver.world()
py_world = mim.PyWorld(world)
sym = mim.Sym()
sympool = mim.SymPool()
test_str = sympool.sym(f"%core.nat.add")
ast = mim.AST(driver.world())
parser = mim.PyParser(mim.Parser(ast))
# parser.plugin("0x3863800000000000")
parser.plugin("core")
parser.plugin("mem")
i32_type = py_world.type_i32()
# argv_t = py_world.implicit_app(i32_type)
ann = py_world.type_i32()
annex = py_world.annex(test_str)
mem_m = sympool.sym(f"mem.M")
mem_t = py_world.annex(mem_m)
print(annex)
print(sym)
print(mem_t)
print(ann)
print(ast)
print(test_str)
assert(False, test_str.empty())
print(test_str.view())
#print(f"is sym empty?: {sym.empty()}")
assert(True, sym.empty())
#print(driver)
py_world.write()
