import sys
import os
# Add our build dir to the python modules list
build_dir = os.path.abspath("../../../../build/lib/")
sys.path.insert(0, build_dir)

import mim  # Import from that list
# this is currently very rudimentary, actual test cases will follow

driver = mim.Driver()
world  = driver.world()
py_world = mim.PyWorld(world)
sym = mim.Sym()
sympool = mim.SymPool()
test_str = sympool.sym("core")
ast = mim.AST(driver.world())
parser = mim.Parser(ast)

driver.log().set_stdout().set(mim.Level.Error)

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
