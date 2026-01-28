
from typing import List

def call(self, callee, *args)->mim.Def:

    if(len(args) == 0):
        return self.annex(callee)
    
    if(isinstance(args[0], List[mim.Def])):
        return self.call(self.implicit_app(callee, args[0]), args[1::])
    
    if(isinstance(args[0], mim.Def)):
        return self.implicit_app(callee, args[0])
    
    raise TypeError("The given arguments dont match the expected types")
    
mim.Def.call = call