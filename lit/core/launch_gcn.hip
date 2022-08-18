// /opt/rocm/llvm/bin/clang++ ../launch_gcn.hip --offload-arch=gfx90c -lamdhip64 -L/opt/rocm/hip/lib/ && ./a.out

#include <cstdint>

#include <algorithm>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include <hip/hip_runtime.h>
#include <string.h>

#define HIP_CHECK(code)                                                                \
    {                                                                                  \
        auto err = (code);                                                             \
        if (err != hipSuccess) { throw std::runtime_error{"error executing " #code " with error " + std::to_string(err) + ": " + hipGetErrorString(err)}; } \
    }

int main(int argc, const char** argv) {
    try {
        std::string module_path{"test.hsaco"};
        std::string kernel_name{"kernel"};
        if (argc > 1) module_path = argv[1];
        if (argc > 2) kernel_name = argv[2];

        std::cout << "load module " << module_path << std::endl;
        hipModule_t module;
        HIP_CHECK(hipModuleLoad(&module, module_path.c_str()));

        std::cout << "get kernel " << kernel_name << std::endl;
        hipFunction_t kernel;
        HIP_CHECK(hipModuleGetFunction(&kernel, module, kernel_name.c_str()));

        std::vector<uint8_t> vals(100);
        hipDeviceptr_t devPtr;
        HIP_CHECK(hipMalloc(&devPtr, 100));

        std::iota(vals.begin(), vals.end(), 0);
        HIP_CHECK(hipMemcpy(devPtr, vals.data(), 100, hipMemcpyHostToDevice));

        std::vector<void*> argBuffer(1);
        memcpy(&argBuffer[0], &devPtr, sizeof(void*));

        size_t size = argBuffer.size() * sizeof(void*);

        void* config[] = {HIP_LAUNCH_PARAM_BUFFER_POINTER, &argBuffer[0], HIP_LAUNCH_PARAM_BUFFER_SIZE, &size,
                          HIP_LAUNCH_PARAM_END};

        HIP_CHECK(hipModuleLaunchKernel(kernel, 4, 1, 1, 25, 1, 1, 0, 0, nullptr, (void**)&config));

        HIP_CHECK(hipDeviceSynchronize());
        HIP_CHECK(hipMemcpy(vals.data(), devPtr, 100, hipMemcpyDeviceToHost));

        std::for_each(vals.begin(), vals.end(),
                      [](uint8_t v) { std::cout << "val: " << static_cast<int>(v) << std::endl; });

        hipModuleUnload(module);
    } catch (const std::runtime_error& err) {
        std::cout << err.what() << std::endl;
        return 1;
    }
    return 0;
}
