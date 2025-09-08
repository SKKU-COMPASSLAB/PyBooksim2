# import os
# import sys
# import ctypes
# from pathlib import Path

# _pkg_dir = Path(__file__).resolve().parent
# _libdir = _pkg_dir / "lib"

# if sys.platform == "win32":
#     os.add_dll_directory(str(_libdir))   # Windows
# else:
#     dep = next(_libdir.glob("libbooksim2.so"), None)
#     if dep:
#         ctypes.CDLL(str(dep), mode=getattr(ctypes, "RTLD_GLOBAL", os.RTLD_GLOBAL))

from .pybooksim2 import *
