clang++ lib.cpp loopDiff.ll  -o loopDiff  -lc -Wno-override-module
clang++ lib.cpp loopDiff2.ll -o loopDiff2 -lc -Wno-override-module
clang++ lib.cpp loopDiff_alloca2.ll  -o loopDiff_alloca  -lc -Wno-override-module
clang++ lib.cpp loopDiff2_alloca2.ll -o loopDiff2_alloca -lc -Wno-override-module
