cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(HSATest)

# fixed path for rocm lib and header for now
set ( HSA_PATH /opt/rocm/hsa )
message( STATUS "HSA_PATH:" ${HSA_PATH} )

set ( ROCM_PATH /opt/rocm )
message( STATUS "ROCM_PATH:" ${ROCM_PATH} )

set ( HSA_INCLUDE_DIRS ${HSA_PATH}/include/hsa )
set ( HSA_LIBRARY_DIRS ${ROCM_PATH}/lib )
set ( HSA_LIBRARY_RUNTIME hsa-runtime64 )

message ( STATUS "To find hsa lib at ${HSA_LIBRARY_DIRS}" )
message ( STATUS "To find hsa include at ${HSA_INCLUDE_DIRS}" )

include_directories( ${PROJECT_SOURCE_DIR}/include )
include_directories( ${HSA_INCLUDE_DIRS} )

aux_source_directory( ${PROJECT_SOURCE_DIR}/src SRC_FILES )

# cmake project source dir
message( STATUS "PROJECT_SOURCE_DIR:" ${PROJECT_SOURCE_DIR} )

add_definitions(-Wall -std=c++11)
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2" )
else ()
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g" )
endif ()

# link_directories() has to be put before add_executable()
link_directories( ${HSA_LIBRARY_DIRS} )

add_executable( hsaReplayer ${SRC_FILES} )
target_link_libraries( hsaReplayer ${HSA_LIBRARY_RUNTIME} )
