add_llvm_loadable_module(LLVMCheckMerge
        DependenceCollector.cpp
        SourceVariableMapper.cpp
)

target_compile_features(LLVMCheckMerge PRIVATE cxx_range_for cxx_auto_type)

set_target_properties(LLVMCheckMerge PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
)

if(APPLE)
    set_target_properties(LLVMCheckMerge PROPERTIES
            LINK_FLAGS "-undefined dynamic_lookup"
            )
endif(APPLE)
