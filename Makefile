.PHONY: build compile

# NOTE: use flag "-G Ninja" to generate compile_commands.json on Windows
build:
	cmake -S . -B build \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_PREFIX="C:\Users\janni\OneDrive\Dokumente\Projects\C, C++\MimIR\build\install" \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1

compile:
	cmake --build build -j 8 -t install

