import sys
import os
from pathlib import Path
# Add our build dir to the python modules list
build_dir = os.path.abspath("../../../../build/lib/")
sys.path.insert(0, build_dir)

import mim

world = mim.Mim().init("core")

mem = world.sym("%mem.M")
world.call(mem, [mem])