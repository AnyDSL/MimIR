add_mim_plugin(sflow
    SOURCES
        cfg.cpp
        # reduc_cfg.cpp  # pre-existing compile errors
        sflow.cpp
        normalizers.cpp
        phase/structurize.cpp
    INSTALL
)
