cmake_minimum_required(VERSION 3.12)
project(xlb)

set(CMAKE_CXX_STANDARD 17)

include_directories(. ./utils clibs /usr/local/include/dpdk)

add_compile_options(-msse4 -mavx)

#add_executable(xlb main.cpp ctrl.h ctrl.cc utils/format.h utils/format.cc utils/cuckoo_map.h utils/common.h)
add_executable(xlb main.cpp utils/allocator.h utils/cuckoo_map.h utils/common.h clibs/hash.h clibs/c_hash.h clibs/c_hash.c)

target_link_libraries(xlb rte_ring rte_eal rte_kvargs pthread numa ${CMAKE_DL_LIBS})
