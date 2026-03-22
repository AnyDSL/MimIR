# Build pipeline: MimIR GEMM kernel → IRON NPU xclbin
#
# Prerequisites:
#   - MimIR built at MIMIR_BUILD (default: ../../../build)
#   - Peano (llvm-aie) installed via ironenv or at PEANO_DIR
#   - ironenv activated for design.mlir and xclbin steps
#
# Usage:
#   make                    # full build → build/final.xclbin (element-wise MAC)
#   make matmul             # full build → build/final_matmul.xclbin (mac_4x4x8)
#   make build/gemm_tile.ll # MimIR → LLVM IR only (no ironenv needed)
#   make build/gemm_tile.o  # + Peano cross-compile (no ironenv needed)
#   make clean

MIMIR_BUILD ?= ../../../build
PEANO_DIR   ?= $(shell python3 -c "import llvm_aie; print(llvm_aie.get_install_dir())" 2>/dev/null)

.PHONY: all matmul tiled clean check-peano

all: build/final.xclbin

matmul: build/final_matmul.xclbin

tiled: build/final_32x32x32.xclbin

# === Element-wise MAC kernel (existing) ===

# Step 1: MimIR → LLVM IR
build/gemm_tile.ll: gemm_tile_iron.mim | build
	$(MIMIR_BUILD)/bin/mim $< --output-ll $@

# Step 2: LLVM IR → AIE2P ELF object (Peano cross-compiler)
build/gemm_tile.o: build/gemm_tile.ll | check-peano
	$(PEANO_DIR)/bin/clang++ -c -O2 --target=aie2p-none-unknown-elf -x ir $< -o $@ -Wno-override-module

# Step 3: IRON Python → MLIR (requires ironenv)
build/design.mlir: design.py | build
	python3 $< > $@

# Step 4: MLIR + kernel .o → xclbin + DMA instructions (requires ironenv + aiecc.py)
build/final.xclbin: build/design.mlir build/gemm_tile.o
	cd build && aiecc.py --alloc-scheme=basic-sequential \
		--aie-generate-xclbin --no-compile-host --xclbin-name=final.xclbin \
		--no-xchesscc --no-xbridge --peano $(PEANO_DIR) \
		--aie-generate-npu-insts --npu-insts-name=insts.txt \
		design.mlir

# === Matmul MAC kernel (mac_4x4x8) ===

build/gemm_tile_matmul.ll: gemm_tile_matmul.mim | build
	$(MIMIR_BUILD)/bin/mim $< --output-ll $@

build/gemm_tile_matmul.o: build/gemm_tile_matmul.ll | check-peano
	$(PEANO_DIR)/bin/clang++ -c -O2 --target=aie2p-none-unknown-elf -x ir $< -o $@ -Wno-override-module

build/design_matmul.mlir: design_matmul.py | build
	python3 $< > $@

build/final_matmul.xclbin: build/design_matmul.mlir build/gemm_tile_matmul.o
	cd build && cp gemm_tile_matmul.o gemm_tile.o && \
	aiecc.py --alloc-scheme=basic-sequential \
		--aie-generate-xclbin --no-compile-host --xclbin-name=final_matmul.xclbin \
		--no-xchesscc --no-xbridge --peano $(PEANO_DIR) \
		--aie-generate-npu-insts --npu-insts-name=insts_matmul.txt \
		design_matmul.mlir

# === 32x32x32 tiled matmul ===

build/gemm_tile_32x32x32.ll: gemm_tile_32x32x32.mim | build
	$(MIMIR_BUILD)/bin/mim $< --output-ll $@

build/gemm_tile_32x32x32.o: build/gemm_tile_32x32x32.ll | check-peano
	$(PEANO_DIR)/bin/clang++ -c -O2 --target=aie2p-none-unknown-elf -x ir $< -o $@ -Wno-override-module

build/design_32x32x32.mlir: design_32x32x32.py | build
	python3 $< > $@

build/final_32x32x32.xclbin: build/design_32x32x32.mlir build/gemm_tile_32x32x32.o
	cd build && cp gemm_tile_32x32x32.o gemm_tile.o && \
	aiecc.py --alloc-scheme=basic-sequential \
		--aie-generate-xclbin --no-compile-host --xclbin-name=final_32x32x32.xclbin \
		--no-xchesscc --no-xbridge --peano $(PEANO_DIR) \
		--aie-generate-npu-insts --npu-insts-name=insts_32x32x32.txt \
		design_32x32x32.mlir

build:
	mkdir -p build

check-peano:
	@if [ -z "$(PEANO_DIR)" ] || [ ! -d "$(PEANO_DIR)" ]; then \
		echo "Error: PEANO_DIR not found. Activate ironenv or set PEANO_DIR."; \
		exit 1; \
	fi

clean:
	rm -rf build
