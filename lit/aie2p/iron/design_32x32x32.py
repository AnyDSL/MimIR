"""IRON design for single-core AIE2P 32x32x32 tiled GEMM kernel using mac_4x4x8.

Generates MLIR for a minimal NPU2 design that:
  - Transfers A (64 sub-tiles of 16 i32 = 4096 bytes) from host to compute
  - Transfers B (32 sub-tiles of 32 i16 = 2048 bytes) from host to compute
  - Calls the MimIR-compiled gemm_tile kernel (triple-nested loops, 256 MAC calls)
  - Transfers C (32 sub-tiles of 32 i16 = 2048 bytes) back to host

Usage (requires ironenv):
  python3 design_32x32x32.py > build/design_32x32x32.mlir
"""

import numpy as np

from aie.iron import Kernel, ObjectFifo, Program, Runtime, Worker
from aie.iron.placers import SequentialPlacer
from aie.iron.device import NPU2Col1

# --- Tile buffer types (what the kernel sees) ---
# A: 64 sub-tiles (rowA=8 × colA=8), each 16 i32 (= 4×4 i16 packed as 16 i32)
a_ty = np.ndarray[(64, 16), np.dtype[np.int32]]
# B: 32 sub-tiles (colA=8 × colB=4), each 32 i16 (= 4×8 i16)
b_ty = np.ndarray[(32, 32), np.dtype[np.int16]]
# C: 32 sub-tiles (rowA=8 × colB=4), each 32 i16 (= 4×8 i16 output)
c_ty = np.ndarray[(32, 32), np.dtype[np.int16]]

# --- Host buffer types (flat 1D for DMA) ---
A_ty = np.ndarray[(64 * 16,), np.dtype[np.int32]]   # 1024 i32
B_ty = np.ndarray[(32 * 32,), np.dtype[np.int16]]   # 1024 i16
C_ty = np.ndarray[(32 * 32,), np.dtype[np.int16]]   # 1024 i16

# --- Kernel declaration ---
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
