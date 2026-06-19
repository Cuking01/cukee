set(CUKEE_LIBTORCH_ROOT "/opt/libtorch" CACHE PATH "Path to libtorch")
set(CUKEE_CUDA_ROOT "/usr/local/cuda" CACHE PATH "Path to CUDA toolkit")
set(CUKEE_C_COMPILER "/usr/bin/gcc" CACHE FILEPATH "Path to gcc")
set(CUKEE_CXX_COMPILER "/usr/bin/g++" CACHE FILEPATH "Path to g++")

list(APPEND CMAKE_PREFIX_PATH "${CUKEE_LIBTORCH_ROOT}")

set(CMAKE_C_COMPILER "${CUKEE_C_COMPILER}" CACHE FILEPATH "Path to C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${CUKEE_CXX_COMPILER}" CACHE FILEPATH "Path to C++ compiler" FORCE)
set(CUDAToolkit_ROOT "${CUKEE_CUDA_ROOT}" CACHE PATH "Path to CUDA toolkit" FORCE)
set(CMAKE_CUDA_COMPILER "${CUKEE_CUDA_ROOT}/bin/nvcc" CACHE FILEPATH "Path to nvcc" FORCE)
