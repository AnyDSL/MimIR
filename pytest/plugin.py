import sys
import os
import subprocess
from pathlib import Path
from abc import ABC, abstractmethod
from jit import JIT
# Add our build dir to the python modules list
build_dir = os.path.abspath("../../../../build/lib/")
sys.path.insert(0, build_dir)

class MimError(Exception):
    pass

class MimPlugin(JIT):
    
    def __init__(self, driver, log_level):
        self.driver = driver
        self.driver.log().set_stdout().set(log_level)
        self.registered_functions = {}
        
    @abstractmethod
    def build():
        raise MemoryError("Needs to be implemented by plugin author")
        
    def register_func():
        pass