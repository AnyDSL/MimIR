.PHONY: build compile

build:
	cmake -S . -B build \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_PREFIX="C:\Users\janni\OneDrive\Dokumente\Projects\C, C++\MimIR\build\install" \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1

compile:
	cmake --build build -j 8 -t install

