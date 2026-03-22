"""Host test for AIE2P GEMM tile kernel using mac_4x4x8 matmul on NPU hardware.

Loads the xclbin and instruction binary, fills A and B with test patterns,
runs the matmul kernel, and verifies output.

The mac_4x4x8 intrinsic performs a 4x4x8 matmul (conf=26, unsigned):
  A is 32 i16 values packed as 16 i32 (512 bits)
  B is 32 i16 values (512 bits)
  acc is v32i64 (ACC2048)

Usage:
  python3 test_matmul.py -x build/final.xclbin -i build/insts.txt

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

K = 4          # number of MAC iterations
A_ELEMS = K * 16   # 64 i32 values (= K * 16 packed i32, each holding 2 i16)
B_ELEMS = K * 32   # 128 i16 values
C_ELEMS = 32       # 32 i16 output values


def load_instructions(filepath):
    """Auto-detect format and load instruction sequence."""
    try:
        with open(filepath, "r") as f:
            return [int(line, 16) for line in f if line.strip()]
    except (ValueError, UnicodeDecodeError):
        size = os.path.getsize(filepath)
        with open(filepath, "rb") as f:
            return list(struct.unpack(f"{size // 4}I", f.read()))


def main():
    parser = argparse.ArgumentParser(description="AIE2P matmul GEMM tile NPU test")
    parser.add_argument("-x", "--xclbin", required=True, help="Path to .xclbin")
    parser.add_argument("-i", "--instr", required=True, help="Path to instruction file")
    parser.add_argument("-k", "--kernel", default="MLIR_AIE", help="Kernel name")
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

    # Fill inputs: all ones
    # A: 64 i32 values — each i32 packs two i16=1 values: 0x00010001 = 65537
    buf_a = np.full(A_ELEMS, 0x00010001, dtype=np.int32)
    # B: 128 i16 values — all ones
    buf_b = np.ones(B_ELEMS, dtype=np.int16)
    bo_a.write(buf_a, 0)
    bo_b.write(buf_b, 0)
    bo_instr.write(np.array(instr_v, dtype=np.uint32), 0)

    # Sync to device
    bo_instr.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
    bo_a.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
    bo_b.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)

    # Warmup run
    print("Warmup run...")
    run = kernel(3, bo_instr, len(instr_v), bo_a, bo_b, bo_c)
    r = run.wait()
    assert r == xrt.ert_cmd_state.ERT_CMD_STATE_COMPLETED, f"Warmup failed with state: {r}"

    # Re-sync buffers for timed run
    bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE)
    bo_instr.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
    bo_a.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
    bo_b.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)

    # Timed run
    print("Timed run...")
    t0 = time.perf_counter()
    run = kernel(3, bo_instr, len(instr_v), bo_a, bo_b, bo_c)
    r = run.wait()
    t1 = time.perf_counter()
    assert r == xrt.ert_cmd_state.ERT_CMD_STATE_COMPLETED, f"Kernel failed with state: {r}"
    latency_us = (t1 - t0) * 1e6
    # mac_4x4x8: 4*4*8=128 MACs per call, K=4 iterations
    macs = 2 * K * 4 * 4 * 8  # multiply-accumulate = 2 ops
    print(f"Latency: {latency_us:.1f} us, MACs: {macs}, GFLOPS: {macs / (1000 * latency_us):.6f}")

    # Read back results
    bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE)
    buf_c = bo_c.read(C_ELEMS * 2, 0)
    result = np.frombuffer(buf_c, dtype=np.int16).copy()

    print(f"Output C[0:32]: {result}")

    # Compute reference: 4x4x8 matmul with all-ones
    # mac_4x4x8 = M=4, K_inner=4, N=8 (4x4 * 4x8 = 4x8)
    # C[i][j] = sum_{k=0}^{3} A[i][k] * B[k][j]
    # For a single mac_4x4x8 with all-ones: C[i][j] = 4 (inner dim K_inner=4)
    # Over K=4 outer iterations: C[i][j] = 4 * 4 = 16
    # Output is 4x8 = 32 i16 values.
    K_inner = 4  # matmul inner dimension
    expected = np.full(C_ELEMS, K * K_inner, dtype=np.int16)

    print(f"Expected:       {expected}")
    if np.array_equal(result, expected):
        print("PASS: output matches reference")
    else:
        print("FAIL: output mismatch")
        print(f"  diff: {result.astype(np.int32) - expected.astype(np.int32)}")
        sys.exit(1)


if __name__ == "__main__":
    main()
