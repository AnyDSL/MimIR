"""Host test for AIE2P 32x32x32 tiled GEMM kernel on NPU hardware.

Loads the xclbin and instruction binary, fills A and B with test patterns,
runs the tiled matmul kernel, and verifies output.

Sub-tile layout:
  A: 64 sub-tiles (rowA=8 × colA=8), each 16 i32 (4×4 i16 packed as 16 i32)
  B: 32 sub-tiles (colA=8 × colB=4), each 32 i16 (4×8 i16)
  C: 32 sub-tiles (rowA=8 × colB=4), each 32 i16 (4×8 i16 output)

The kernel performs 256 mac_4x4x8 calls (8×4×8 loop nest), 65536 FLOPs total.

Usage:
  python3 test_32x32x32.py -x build/final_32x32x32.xclbin -i build/insts_32x32x32.txt

Requires: pyxrt (from /opt/xilinx/xrt/python/)
"""

import argparse
import os
import struct
import sys
import time

import numpy as np

# pyxrt lives at /opt/xilinx/xrt/python/ — ensure it's importable
XRT_PYTHON = "/opt/xilinx/xrt/python"
if XRT_PYTHON not in sys.path:
    sys.path.insert(0, XRT_PYTHON)

import pyxrt as xrt

# Tile dimensions
M, K, N = 32, 32, 32
r, s, t = 4, 4, 8  # mac_4x4x8 microkernel shape
rowA = M // r       # 8
colA = K // s       # 8
colB = N // t       # 4

# Buffer sizes
A_ELEMS = rowA * colA * 16   # 64 * 16 = 1024 i32
B_ELEMS = colA * colB * 32   # 32 * 32 = 1024 i16
C_ELEMS = rowA * colB * 32   # 32 * 32 = 1024 i16


def load_instructions(filepath):
    """Auto-detect format and load instruction sequence."""
    try:
        with open(filepath, "r") as f:
            return [int(line, 16) for line in f if line.strip()]
    except (ValueError, UnicodeDecodeError):
        size = os.path.getsize(filepath)
        with open(filepath, "rb") as f:
            return list(struct.unpack(f"{size // 4}I", f.read()))


def pack_a_subtiles(A_host):
    """Pack a 32×32 i16 matrix into 64 sub-tiles of 16 i32 (4×4 i16 packed as i32).

    Sub-tile [z*colA + k] holds rows z*4..z*4+3, cols k*4..k*4+3.
    Each pair of consecutive i16 values is packed into one i32: low | (high << 16).
    """
    buf = np.zeros(A_ELEMS, dtype=np.int32)
    for z in range(rowA):
        for k in range(colA):
            tile_idx = z * colA + k
            block = A_host[z*r:(z+1)*r, k*s:(k+1)*s]  # 4×4 i16
            # Flatten to 16 i16, pack pairs into 8 i32... but mac_4x4x8 takes 16 i32
            # The hardware reads 512 bits = 16 i32. The 4×4 block is 16 i16 = 256 bits.
            # Pack: each i32 holds one pair of i16 values
            flat = block.flatten().astype(np.uint16)  # 16 i16
            packed = np.zeros(16, dtype=np.int32)
            for i in range(0, 16, 2):
                packed[i // 2] = int(flat[i]) | (int(flat[i + 1]) << 16)
            # Remaining 8 i32 are don't-care (zero)
            buf[tile_idx * 16: tile_idx * 16 + 16] = packed
    return buf


def pack_b_subtiles(B_host):
    """Pack a 32×32 i16 matrix into 32 sub-tiles of 32 i16 (4×8 blocks).

    Sub-tile [k*colB + j] holds rows k*4..k*4+3, cols j*8..j*8+7.
    """
    buf = np.zeros(B_ELEMS, dtype=np.int16)
    for k in range(colA):
        for j in range(colB):
            tile_idx = k * colB + j
            block = B_host[k*s:(k+1)*s, j*t:(j+1)*t]  # 4×8 i16
            buf[tile_idx * 32: tile_idx * 32 + 32] = block.flatten()
    return buf


def unpack_c_subtiles(buf_c):
    """Unpack 32 sub-tiles of 32 i16 into a 32×32 i16 matrix.

    Sub-tile [z*colB + j] holds rows z*4..z*4+3, cols j*8..j*8+7.
    """
    C_host = np.zeros((M, N), dtype=np.int16)
    for z in range(rowA):
        for j in range(colB):
            tile_idx = z * colB + j
            block = buf_c[tile_idx * 32: tile_idx * 32 + 32].reshape(r, t)
            C_host[z*r:(z+1)*r, j*t:(j+1)*t] = block
    return C_host


def main():
    parser = argparse.ArgumentParser(description="AIE2P 32x32x32 tiled GEMM NPU test")
    parser.add_argument("-x", "--xclbin", required=True, help="Path to .xclbin")
    parser.add_argument("-i", "--instr", required=True, help="Path to instruction file")
    parser.add_argument("-k", "--kernel", default="MLIR_AIE", help="Kernel name")
    parser.add_argument("--structured", action="store_true",
                        help="Run structured test with sub-tile indexing verification")
    parser.add_argument("--warmup", type=int, default=1, help="Number of warmup iterations")
    parser.add_argument("--iters", type=int, default=1, help="Number of timed iterations")
    args = parser.parse_args()

    # Load instructions
    instr_v = load_instructions(args.instr)
    print(f"Loaded {len(instr_v)} instructions from {args.instr}")

    # Set up XRT device and kernel
    device = xrt.device(0)
    xclbin = xrt.xclbin(args.xclbin)
    device.register_xclbin(xclbin)
    context = xrt.hw_context(device, xclbin.get_uuid())
    xkernels = xclbin.get_kernels()
    xkernel = [k for k in xkernels if args.kernel in k.get_name()][0]
    kernel = xrt.kernel(context, xkernel.get_name())

    # Allocate buffer objects
    bo_instr = xrt.bo(device, len(instr_v) * 4, xrt.bo.cacheable, kernel.group_id(1))
    bo_a = xrt.bo(device, A_ELEMS * 4, xrt.bo.host_only, kernel.group_id(3))   # i32 = 4 bytes
    bo_b = xrt.bo(device, B_ELEMS * 2, xrt.bo.host_only, kernel.group_id(4))   # i16 = 2 bytes
    bo_c = xrt.bo(device, C_ELEMS * 2, xrt.bo.host_only, kernel.group_id(5))   # i16 = 2 bytes

    if args.structured:
        print("=== Structured test (sub-tile indexing verification) ===")
        # Create small-valued matrices to avoid i16 overflow
        np.random.seed(42)
        A_host = np.random.randint(0, 4, size=(M, K), dtype=np.int16)
        B_host = np.random.randint(0, 4, size=(K, N), dtype=np.int16)
        buf_a = pack_a_subtiles(A_host)
        buf_b = pack_b_subtiles(B_host)
        expected_full = A_host.astype(np.int32) @ B_host.astype(np.int32)
        expected = expected_full.astype(np.int16)
        print(f"A sample [0,:8]: {A_host[0,:8]}")
        print(f"B sample [0,:8]: {B_host[0,:8]}")
    else:
        print("=== All-ones test ===")
        # A: all i32 = 0x00010001 (two i16=1 packed per i32)
        buf_a = np.full(A_ELEMS, 0x00010001, dtype=np.int32)
        # B: all i16 = 1
        buf_b = np.ones(B_ELEMS, dtype=np.int16)
        # Expected: each output i16 = K_tile = 32
        # (mac_4x4x8 contributes K_inner=4 per call, × colA=8 iterations in k-loop)
        expected = np.full((M, N), K, dtype=np.int16)

    bo_a.write(buf_a, 0)
    bo_b.write(buf_b, 0)
    bo_instr.write(np.array(instr_v, dtype=np.uint32), 0)

    # Sync to device
    bo_instr.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
    bo_a.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
    bo_b.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)

    # Warmup runs
    print(f"Warmup ({args.warmup} iterations)...")
    for _ in range(args.warmup):
        run = kernel(3, bo_instr, len(instr_v), bo_a, bo_b, bo_c)
        state = run.wait()
        assert state == xrt.ert_cmd_state.ERT_CMD_STATE_COMPLETED, f"Warmup failed with state: {state}"

    # 256 mac_4x4x8 calls, each: 4*4*8=128 MACs, ×2 for multiply-accumulate
    total_flops = 2 * rowA * colB * colA * r * s * t  # 2 * 8 * 4 * 8 * 4 * 4 * 8 = 65536

    # Timed runs
    print(f"Timed runs ({args.iters} iterations)...")
    zero_c = np.zeros(C_ELEMS * 2, dtype=np.uint8)
    times_us = []
    for i in range(args.iters):
        bo_c.write(zero_c, 0)
        bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
        bo_instr.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
        bo_a.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
        bo_b.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
        t0 = time.perf_counter()
        run = kernel(3, bo_instr, len(instr_v), bo_a, bo_b, bo_c)
        state = run.wait()
        t1 = time.perf_counter()
        assert state == xrt.ert_cmd_state.ERT_CMD_STATE_COMPLETED, f"Iteration {i} failed: {state}"
        bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE)
        times_us.append((t1 - t0) * 1e6)

    avg_us = sum(times_us) / len(times_us)
    min_us = min(times_us)
    max_us = max(times_us)
    print(f"\nAvg NPU matmul time: {avg_us:.1f} us")
    print(f"Avg NPU gflops: {total_flops / (1000 * avg_us):.6f}")
    print(f"\nMin NPU matmul time: {min_us:.1f} us")
    print(f"Max NPU gflops: {total_flops / (1000 * min_us):.6f}")
    print(f"\nMax NPU matmul time: {max_us:.1f} us")
    print(f"Min NPU gflops: {total_flops / (1000 * max_us):.6f}")

    # Read back results
    bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE)
    raw_c = bo_c.read(C_ELEMS * 2, 0)
    result_flat = np.frombuffer(raw_c, dtype=np.int16).copy()
    result = unpack_c_subtiles(result_flat)

    print(f"Output C[0,:8]: {result[0,:8]}")
    print(f"Expected[0,:8]: {expected[0,:8]}")

    if np.array_equal(result, expected):
        print("PASS: output matches reference")
    else:
        diff = result.astype(np.int32) - expected.astype(np.int32)
        mismatches = np.count_nonzero(diff)
        print(f"FAIL: {mismatches}/{M*N} elements mismatch")
        print(f"  max abs diff: {np.max(np.abs(diff))}")
        # Show first few mismatches
        rows, cols = np.where(diff != 0)
        for idx in range(min(10, len(rows))):
            r_i, c_i = rows[idx], cols[idx]
            print(f"  C[{r_i},{c_i}] = {result[r_i,c_i]}, expected {expected[r_i,c_i]}")
        sys.exit(1)


if __name__ == "__main__":
    main()
