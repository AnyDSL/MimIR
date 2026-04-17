from abc import ABC
import platform as pf
from ctypes import cdll
import subprocess
import os

class JIT(ABC):
    
    def __init__(self, so_name):
        self.so_name = so_name
    
    def jit(self):
        platform = pf.platform()
        windows = platform.find("Windows")
        ext = ""
        if (windows != -1):
            ext = ".dll"
        else:
            ext = ".so"
        name = self.so_name + ext
        so = cdll.LoadLibrary(name)
        so.
        pass
    
    