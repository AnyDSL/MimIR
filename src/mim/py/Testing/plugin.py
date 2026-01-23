import sys
import os
import subprocess
from pathlib import Path
# Add our build dir to the python modules list
build_dir = os.path.abspath("../../../../build/lib/")
sys.path.insert(0, build_dir)

import mim
class MimError(Exception):
    pass

class MimPlugin():

    def __init__(driver, log_level):
        driver.log().set_stdout().set(log_level)
    
    def build():
        raise MemoryError("Needs to be implemented by plugin author")
