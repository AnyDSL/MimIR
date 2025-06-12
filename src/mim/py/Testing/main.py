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
driver.add_search_path(Path("../../../../build/include/mim/plug/mem/autogen.h"))
world  = driver.world()
py_world = mim.PyWorld(world)
sym = mim.Sym()
sympool = mim.SymPool()
test_str = sympool.sym("core")
test_str = sympool.sym("core")
ast = mim.AST(driver.world())
parser = mim.PyParser(mim.Parser(ast))
#parser.plugin("0x3863800000000000")
driver.log().set_stdout().set(mim.Level.Error)
argv_t = py_world.implicit_app(py_world.type_i32())
print(sym)
print(ast)
print(test_str)
#print(f"is the test string empty?: {test_str.empty()}")
assert(False, test_str.empty())
print(test_str.view())
#print(f"is sym empty?: {sym.empty()}")
assert(True, sym.empty())
#print(driver)
py_world.write()
