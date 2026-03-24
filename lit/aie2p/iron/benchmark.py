#!/usr/bin/env python3
"""Benchmark: MimIR vs mlir-aie reference for 32x32x32 single-core i16 GEMM."""

import argparse
import numpy as np
import time
import sys
import os

def run_kernel(xclbin_path, instr_path, a_buf, b_buf, c_buf, warmup=10, iters=100):
    """Run a kernel and return latencies in microseconds."""
    import pyxrt as xrt

    with open(instr_path, "r") as f:
        instr_text = f.read().split("\n")
        instr_text = [l for l in instr_text if l != ""]
        instr_v = np.array([int(i, 16) for i in instr_text], dtype=np.uint32)

    device = xrt.device(0)
    xclbin = xrt.xclbin(xclbin_path)
    device.register_xclbin(xclbin)
    context = xrt.hw_context(device, xclbin.get_uuid())

    kernel = xrt.kernel(context, "MLIR_AIE")

    bo_instr = xrt.bo(device, len(instr_v) * 4, xrt.bo.cacheable, kernel.group_id(1))
    bo_a = xrt.bo(device, a_buf.nbytes, xrt.bo.host_only, kernel.group_id(3))
    bo_b = xrt.bo(device, b_buf.nbytes, xrt.bo.host_only, kernel.group_id(4))
    bo_c = xrt.bo(device, c_buf.nbytes, xrt.bo.host_only, kernel.group_id(5))

    bo_instr.write(instr_v, 0)
    bo_instr.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_DIR_TO_DEVICE)
    bo_a.write(a_buf, 0)
    bo_a.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_DIR_TO_DEVICE)
    bo_b.write(b_buf, 0)
    bo_b.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_DIR_TO_DEVICE)

    # Warmup
    for _ in range(warmup):
        bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_DIR_TO_DEVICE)
        kernel(bo_instr, len(instr_v), bo_a, bo_b, bo_c)
        bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_DIR_FROM_DEVICE)

    # Timed runs
    latencies = []
    for _ in range(iters):
        bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_DIR_TO_DEVICE)
        t0 = time.perf_counter()
        kernel(bo_instr, len(instr_v), bo_a, bo_b, bo_c)
        bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_DIR_FROM_DEVICE)
        t1 = time.perf_counter()
        latencies.append((t1 - t0) * 1e6)

    c_out = np.zeros_like(c_buf)
    bo_c.read(c_out, 0)
    return latencies, c_out


def pack_a_subtiles(A, M=32, K=32, tile_m=4, tile_k=4):
    """Pack A (M×K i16) into sub-tiles of «16; I32» (4×4 i16 packed as 16 i32)."""
    rows = M // tile_m
    cols = K // tile_k
    # each sub-tile: 4×4 i16 → 8 i32 → padded to 16 i32 (only first 8 used by hardware)
    packed = np.zeros(rows * cols * 16, dtype=np.int32)
    for r in range(rows):
        for c in range(cols):
            block = A[r*tile_m:(r+1)*tile_m, c*tile_k:(c+1)*tile_k].astype(np.int16)
            flat16 = block.flatten()
            # pack pairs of i16 into i32
            flat32 = np.zeros(8, dtype=np.int32)
            for j in range(8):
                lo = int(flat16[2*j]) & 0xFFFF
                hi = int(flat16[2*j+1]) & 0xFFFF
                flat32[j] = lo | (hi << 16)
            idx = (r * cols + c) * 16
            packed[idx:idx+8] = flat32
    return packed


def pack_b_subtiles(B, K=32, N=32, tile_k=4, tile_n=8):
    """Pack B (K×N i16) into sub-tiles of «32; I16»."""
    rows = K // tile_k
    cols = N // tile_n
    packed = np.zeros(rows * cols * 32, dtype=np.int16)
    for r in range(rows):
        for c in range(cols):
            block = B[r*tile_k:(r+1)*tile_k, c*tile_n:(c+1)*tile_n].astype(np.int16)
            idx = (r * cols + c) * 32
            packed[idx:idx+32] = block.flatten()
    return packed


def unpack_c_subtiles(c_flat, M=32, N=32, tile_m=4, tile_n=8):
    """Unpack C sub-tiles «32; I16» back to M×N matrix."""
    rows = M // tile_m
    cols = N // tile_n
    C = np.zeros((M, N), dtype=np.int16)
    for r in range(rows):
        for c in range(cols):
            idx = (r * cols + c) * 32
            block = c_flat[idx:idx+32].reshape(tile_m, tile_n)
            C[r*tile_m:(r+1)*tile_m, c*tile_n:(c+1)*tile_n] = block
    return C


def main():
    parser = argparse.ArgumentParser(description="Benchmark MimIR vs mlir-aie 32x32x32 GEMM")
    parser.add_argument("--mimir-xclbin", required=True, help="MimIR xclbin path")
    parser.add_argument("--mimir-instr", required=True, help="MimIR instr path")
    parser.add_argument("--ref-xclbin", default=None, help="mlir-aie reference xclbin path")
    parser.add_argument("--ref-instr", default=None, help="mlir-aie reference instr path")
    parser.add_argument("--warmup", type=int, default=10)
    parser.add_argument("--iters", type=int, default=100)
    args = parser.parse_args()

    M, K, N = 32, 32, 32
    FLOPS = 2 * M * K * N  # 65536

    # All-ones input
    A = np.ones((M, K), dtype=np.int16)
    B = np.ones((K, N), dtype=np.int16)

    # === MimIR kernel ===
    print(f"=== MimIR 32x32x32 GEMM (warmup={args.warmup}, iters={args.iters}) ===")
    a_packed = pack_a_subtiles(A).view(np.uint8)
    b_packed = pack_b_subtiles(B).view(np.uint8)
    c_buf = np.zeros(M // 4 * N // 8 * 32, dtype=np.int16).view(np.uint8)

    mimir_lats, c_raw = run_kernel(
        args.mimir_xclbin, args.mimir_instr,
        a_packed, b_packed, c_buf,
        warmup=args.warmup, iters=args.iters
    )
    c_mimir = unpack_c_subtiles(c_raw.view(np.int16))
    expected = (A.astype(np.int32) @ B.astype(np.int32)).astype(np.int16)
    mimir_correct = np.array_equal(c_mimir, expected)

    avg_m = np.mean(mimir_lats)
    min_m = np.min(mimir_lats)
    max_m = np.max(mimir_lats)
    print(f"  Avg: {avg_m:.1f} us  ({FLOPS/avg_m/1000:.3f} GFLOPS)")
    print(f"  Min: {min_m:.1f} us  ({FLOPS/min_m/1000:.3f} GFLOPS)")
    print(f"  Max: {max_m:.1f} us")
    print(f"  Correct: {mimir_correct}")

    # === mlir-aie reference ===
    if args.ref_xclbin and args.ref_instr:
        print(f"\n=== mlir-aie reference 32x32x32 GEMM ===")
        # The reference uses row-major i16 layout directly
        a_ref = A.flatten().view(np.uint8)
        b_ref = B.flatten().view(np.uint8)
        c_ref_buf = np.zeros(M * N, dtype=np.int32).view(np.uint8)

        ref_lats, c_ref_raw = run_kernel(
            args.ref_xclbin, args.ref_instr,
            a_ref, b_ref, c_ref_buf,
            warmup=args.warmup, iters=args.iters
        )
        c_ref = c_ref_raw.view(np.int32).reshape(M, N)
        ref_correct = np.array_equal(c_ref, A.astype(np.int32) @ B.astype(np.int32))

        avg_r = np.mean(ref_lats)
        min_r = np.min(ref_lats)
        max_r = np.max(ref_lats)
        print(f"  Avg: {avg_r:.1f} us  ({FLOPS/avg_r/1000:.3f} GFLOPS)")
        print(f"  Min: {min_r:.1f} us  ({FLOPS/min_r/1000:.3f} GFLOPS)")
        print(f"  Max: {max_r:.1f} us")
        print(f"  Correct: {ref_correct}")

        print(f"\n=== Comparison ===")
        print(f"  MimIR avg:    {avg_m:.1f} us")
        print(f"  mlir-aie avg: {avg_r:.1f} us")
        print(f"  Ratio:        {avg_m/avg_r:.2f}x (lower is better for MimIR)")
    else:
        print("\nNo reference xclbin provided. Use --ref-xclbin and --ref-instr to compare.")


if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    main()
