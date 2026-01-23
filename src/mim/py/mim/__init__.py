
try:
    from ._mim_core import Def, Regex, some_internal_logic
except ImportError:
    # Fallback or helpful error if the .so isn't found
    raise ImportError("C++ binary _mim_core not found. Did you build the project?")