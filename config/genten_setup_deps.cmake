if(GENTEN_KOKKOS_DIR)
    
    # check for both lib64 and lib
    if(EXISTS ${GENTEN_KOKKOS_DIR}/lib64/cmake/Kokkos/)
        set(GENTEN_KOKKOS_CMAKE_CONFIG_DIR ${GENTEN_KOKKOS_DIR}/lib64/cmake/Kokkos/)
    endif()

    if(EXISTS ${GENTEN_KOKKOS_DIR}/lib/cmake/Kokkos/)
        set(GENTEN_KOKKOS_CMAKE_CONFIG_DIR ${GENTEN_KOKKOS_DIR}/lib/cmake/Kokkos/)
    endif()

    if(NOT EXISTS ${GENTEN_KOKKOS_CMAKE_CONFIG_DIR}/KokkosConfig.cmake)
        MESSAGE(FATAL_ERROR "Could not find kokkos CMake include file (${GENTEN_KOKKOS_CMAKE_CONFIG_DIR}/KokkosConfig.cmake)")
    endif()

    ###############################################################################
    # Import Kokkos CMake targets
    ###############################################################################
    find_package(Kokkos REQUIRED
                 NO_DEFAULT_PATH
                 COMPONENTS separable_compilation
                 PATHS ${GENTEN_KOKKOS_CMAKE_CONFIG_DIR})

endif()

###############################################################################
# If we are using Cuda, we need to find the cuda toolkit 
#Import Cuda Math Libs
###############################################################################
if(GENTEN_CUDA_ENABLED)
    find_package(CUDAToolkit REQUIRED cublas cusolver)
endif()

