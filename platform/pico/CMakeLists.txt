add_library(nulladpcm INTERFACE)
target_sources(nulladpcm INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/../../src/msm6258.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/msm6258_c.cpp)
target_include_directories(nulladpcm INTERFACE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_compile_features(nulladpcm INTERFACE cxx_std_17)
