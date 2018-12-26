cmake_minimum_required(VERSION 3.12)
project(xlb)

set(CMAKE_CXX_STANDARD 17)

set(GTEST_SRC googletest)
#set(BRPC_SRC brpc)

add_subdirectory(${GTEST_SRC})
#add_subdirectory(${BRPC_SRC})

add_compile_options(-O0 -msse4 -mavx)

set(DPDK_LIBS
        "-Wl,--whole-archive"
        dpdk
        "-Wl,--no-whole-archive"
        )

set(3RD_PARTY_LIBS
        protobuf
        brpc
        gflags
        glog
        )

set(SYS_LIBS
        stdc++fs
        pthread
        numa
        dl
        )

set(GTEST_LIBS
        gtest
        gtest_main
        ${DPDK_LIBS}
        ${SYS_LIBS}
        )

set(DPDK_INCS
        /usr/local/include/dpdk
        )

set(SRC_INCS
        .
        )

set(SYS_INCS
        )

set(GTEST_INCS
        ${GTEST_SRC}/include
        )

set(OBJS
        global_variable.cc
        packet_pool.cc
        dpdk.cc
        worker.cc
        config.cc
        modules.cc
        control.cc
        pb/xlb.pb.cc
        utils/copy.cc
        utils/simd.cc
        utils/numa.cc
        utils/format.cc
        utils/endian.cc
        utils/thread_local.cc
        headers/ip.cc
        headers/ether.cc
        ports/pmd.cc
        )

link_directories(/usr/local/lib)
include_directories(${SRC_INCS} ${DPDK_INCS} ${SYS_INCS} ${GTEST_INC})

#add_executable(runHashTests tests/hash.cc clibs/c_hash.c)
#target_link_libraries(runHashTests ${GTEST_LIBS})

#add_executable(runCuckooMapTests tests/cuckoo_map.cc clibs/c_hash.c)
#target_link_libraries(runCuckooMapTests ${GTEST_LIBS})

#add_executable(xlb main.cpp)
add_executable(xlb3 tests/main3.cc ${OBJS})
#add_executable(packet packet.cc utils/format.cc)
#add_executable(xlb main.cpp clibs/c_hash.c)

#target_link_libraries(xlb glog rte_eal pthread numa ${CMAKE_DL_LIBS})
target_link_libraries(xlb3 ${DPDK_LIBS} ${3RD_PARTY_LIBS} ${SYS_LIBS})
