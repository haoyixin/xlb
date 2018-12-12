cmake_minimum_required(VERSION 3.12)
project(xlb)

set(CMAKE_CXX_STANDARD 17)

set(GTEST_SRC googletest/googletest)
add_subdirectory(${GTEST_SRC})

add_compile_options(-O3 -msse4 -mavx)

set(DPDK_LIBS
        dpdk
        )

set(SYS_LIBS
        gflags
        glog
        pthread
        numa
        ${CMAKE_DL_LIBS}
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
        packet.cc
        packet_pool.cc
        opts.cc
        dpdk.cc
        task.cc
        module.cc
        worker.cc
        scheduler.cc
        port.cc
        config.cc
        utils/copy.cc
        utils/simd.cc
        utils/numa.cc
        utils/format.cc
        utils/time.cc
        utils/endian.cc
        headers/ether.cc
        ports/pmd.cc
        )

include_directories(${SRC_INCS} ${DPDK_INCS} ${SYS_INCS} ${GTEST_INC})

#add_executable(runHashTests tests/hash.cc clibs/c_hash.c)
#target_link_libraries(runHashTests ${GTEST_LIBS})

#add_executable(runCuckooMapTests tests/cuckoo_map.cc clibs/c_hash.c)
#target_link_libraries(runCuckooMapTests ${GTEST_LIBS})

#add_executable(xlb main.cpp)
add_executable(xlb2 main2.cpp ${OBJS})
#add_executable(packet packet.cc utils/format.cc)
#add_executable(xlb main.cpp clibs/c_hash.c)

#target_link_libraries(xlb glog rte_eal pthread numa ${CMAKE_DL_LIBS})
target_link_libraries(xlb2 dpdk gflags glog pthread numa ${CMAKE_DL_LIBS})
