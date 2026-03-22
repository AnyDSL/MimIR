"""IRON design for single-core AIE2P GEMM tile kernel.

Generates MLIR for a minimal NPU2 design that:
  - Transfers one A tile (4x32 i16) and one B tile (4x32 i16) from host to compute
  - Calls the MimIR-compiled gemm_tile kernel
  - Transfers one C tile (16 i32) back to host

Usage (requires ironenv):
  python3 design.py > build/design.mlir
"""

import numpy as np

from aie.iron import Kernel, ObjectFifo, Program, Runtime, Worker
from aie.iron.placers import SequentialPlacer
from aie.iron.device import NPU2Col1

# --- Tile buffer types (what the kernel sees) ---
a_ty = np.ndarray[(4, 32), np.dtype[np.int16]]
b_ty = np.ndarray[(4, 32), np.dtype[np.int16]]
c_ty = np.ndarray[(16,), np.dtype[np.int32]]

# --- Host buffer types (flat 1D for DMA) ---
A_ty = np.ndarray[(4 * 32,), np.dtype[np.int16]]
B_ty = np.ndarray[(4 * 32,), np.dtype[np.int16]]
C_ty = np.ndarray[(16,), np.dtype[np.int32]]

# --- Kernel declaration ---
# Name must match the exported symbol in the .o file.
gemm_kernel = Kernel("gemm_tile", "gemm_tile.o", [a_ty, b_ty, c_ty])

# --- ObjectFifos: host <-> compute tile ---
of_a = ObjectFifo(a_ty, name="inA")
of_b = ObjectFifo(b_ty, name="inB")
of_c = ObjectFifo(c_ty, name="outC")


# --- Worker: acquire tiles, call kernel, release ---
def core_fn(of_a, of_b, of_c, kernel):
    elem_a = of_a.acquire(1)
    elem_b = of_b.acquire(1)
    elem_c = of_c.acquire(1)
    kernel(elem_a, elem_b, elem_c)
    of_a.release(1)
    of_b.release(1)
    of_c.release(1)


worker = Worker(core_fn, [of_a.cons(), of_b.cons(), of_c.prod(), gemm_kernel])

# --- Runtime DMA sequence ---
rt = Runtime()
with rt.sequence(A_ty, B_ty, C_ty) as (A, B, C):
    rt.start(worker)
    rt.fill(of_a.prod(), A)
    rt.fill(of_b.prod(), B)
    rt.drain(of_c.cons(), C, wait=True)

# --- Resolve and emit MLIR ---
module = Program(NPU2Col1(), rt).resolve_program(SequentialPlacer())
print(module)
