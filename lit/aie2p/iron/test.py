"""Host test for AIE2P GEMM tile kernel on NPU hardware.

Loads the xclbin and instruction binary, fills A and B with all-ones,
runs the kernel, and verifies non-zero output.

Usage:
  python3 test.py -x build/final.xclbin -i build/insts.txt

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

A_SIZE = 4 * 32  # 128 i16 values (256 bytes)
B_SIZE = 4 * 32  # 128 i16 values (256 bytes)
C_SIZE = 16      # 16 i32 values  (64 bytes)


def load_instr_binary(filepath):
    """Load DMA instruction sequence from binary file."""
    size = os.path.getsize(filepath)
    with open(filepath, "rb") as f:
        return list(struct.unpack(f"{size // 4}I", f.read()))


def load_instr_text(filepath):
    """Load DMA instruction sequence from text file (one hex value per line)."""
    with open(filepath, "r") as f:
        return [int(line, 16) for line in f if line.strip()]


def load_instructions(filepath):
    """Auto-detect format and load instruction sequence."""
    try:
        return load_instr_text(filepath)
    except (ValueError, UnicodeDecodeError):
        return load_instr_binary(filepath)


def main():
    parser = argparse.ArgumentParser(description="AIE2P GEMM tile NPU test")
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
    bo_a = xrt.bo(device, A_SIZE * 2, xrt.bo.host_only, kernel.group_id(3))   # i16 = 2 bytes
    bo_b = xrt.bo(device, B_SIZE * 2, xrt.bo.host_only, kernel.group_id(4))   # i16 = 2 bytes
    bo_c = xrt.bo(device, C_SIZE * 4, xrt.bo.host_only, kernel.group_id(5))   # i32 = 4 bytes

    # Fill inputs: all ones
    buf_a = np.ones(A_SIZE, dtype=np.int16)
    buf_b = np.ones(B_SIZE, dtype=np.int16)
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
    macs = 2 * 4 * 16  # K=4, 16 lanes (element-wise MAC)
    print(f"Latency: {latency_us:.1f} us, MACs: {macs}, GFLOPS: {macs / (1000 * latency_us):.6f}")

    # Read back results
    bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE)
    buf_c = bo_c.read(C_SIZE * 4, 0)
    result = np.frombuffer(buf_c, dtype=np.int32)

    print(f"Output C[0:16]: {result}")

    # Compute reference: element-wise MAC with conf=90 (mac_elem_16_2)
    # C[i] = sum_{k=0}^{K-1} A[k][i] * B[k][i]  for i=0..15
    # Only the first 16 elements of each 32-element vector are used.
    K = 4
    a_mat = buf_a.reshape(K, 32)
    b_mat = buf_b.reshape(K, 32)
    expected = np.zeros(C_SIZE, dtype=np.int32)
    for k in range(K):
        expected += (a_mat[k, :16].astype(np.int32) * b_mat[k, :16].astype(np.int32))

    print(f"Expected:       {expected}")
    if np.array_equal(result, expected):
        print("PASS: output matches reference")
    else:
        print("FAIL: output mismatch")
        print(f"  diff: {result - expected}")
        sys.exit(1)


if __name__ == "__main__":
    main()
