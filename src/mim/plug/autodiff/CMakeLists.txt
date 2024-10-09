add_mim_plugin(autodiff
    SOURCES
        autodiff.cpp
        pass/autodiff_eval.cpp
        pass/autodiff_zero.cpp
        pass/autodiff_zero_cleanup.cpp
        auxiliary/autodiff_rewrite_inner.cpp
        auxiliary/autodiff_rewrite_toplevel.cpp
        normalizers.cpp
        ../compile/pass/internal_cleanup.cpp # hack
    INSTALL
)
