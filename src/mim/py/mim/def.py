import _mim_core

def call(self, callee, *args):
    if(len(args) == 0):
        return self.annex(callee)

    