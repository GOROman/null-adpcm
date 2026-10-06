idf_component_register(SRCS "../../src/msm6258.cpp" "../../src/msm6258_c.cpp" INCLUDE_DIRS "../../src")
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions -fno-rtti)
