cmake_minimum_required(VERSION 3.10)
project(xlb)

set(CMAKE_CXX_STANDARD 17)

#set(GTEST_SRC googletest)
#set(BRPC_SRC brpc)
#set(ABSL_SRC abseil-cpp)

#add_subdirectory(${GTEST_SRC})
#add_subdirectory(${BRPC_SRC})

#add_compile_options(-O3 -msse4 -mavx)
add_compile_options(-O3 -msse4 -mavx -Wall -Wextra)

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

#set(ABSL_LIBS
#        absl::flat_hash_map
#        )

set(DPDK_INCS
        /usr/local/include/dpdk
        )

set(SRC_INCS
        .
        )

set(SYS_INCS
        )

#set(GTEST_INCS
#        ${GTEST_SRC}/include
#        )

#set(ABSL_INCS
#        ${ABSL_SRC}
#        )

set(OBJS
        scheduler.cc
        packet_pool.cc
        dpdk.cc
        worker.cc
        config.cc
        control.cc
        pb/xlb.pb.cc
        utils/copy.cc
        utils/simd.cc
        utils/numa.cc
        utils/format.cc
        utils/endian.cc
        utils/iface.cc
        headers/ip.cc
        headers/ether.cc
        ports/pmd.cc
        ports/kni.cc
        modules/ether_out.cc
        modules/ether_inc.cc
        modules/arp_inc.cc
        modules/ipv4_inc.cc
        modules/tcp_inc.cc
#        modules/csum.cc
        connection.cc)

link_directories(/usr/local/lib)
include_directories(${SRC_INCS} ${DPDK_INCS} ${SYS_INCS} 3rdparty)

#add_executable(runHashTests tests/hash.cc clibs/c_hash.c)
#target_link_libraries(runHashTests ${GTEST_LIBS})

#add_executable(runCuckooMapTests tests/cuckoo_map.cc clibs/c_hash.c)
#target_link_libraries(runCuckooMapTests ${GTEST_LIBS})

#add_subdirectory(abseil-cpp)

#add_executable(xlb main.cpp)
#add_executable(xlb3 tests/main3.cc ${OBJS})
add_executable(xlb xlb.cc ${OBJS})
#add_executable(xlb5 tests/main5.cc)
#add_executable(xlb3 tests/main3.cc)
#add_executable(packet packet.cc utils/format.cc)
#add_executable(xlb main.cpp clibs/c_hash.c)

set_property(TARGET xlb PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
#target_link_options(xlb5 PRIVATE -Winline)

#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
#SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
#SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")

#target_link_libraries(xlb glog rte_eal pthread numa ${CMAKE_DL_LIBS})
target_link_libraries(xlb ${DPDK_LIBS} ${3RD_PARTY_LIBS} ${SYS_LIBS})
#target_link_libraries(xlb5 pthread)
#target_link_libraries(xlb4)
