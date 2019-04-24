cmake_minimum_required(VERSION 3.10)
project(xlb)

set(CMAKE_CXX_STANDARD 17)

#add_subdirectory(${GTEST_SRC})
#add_subdirectory(${BRPC_SRC})

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
#        gtest_main
#        ${DPDK_LIBS}
#        ${SYS_LIBS}
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
        runtime/scheduler.cc
        runtime/packet_pool.cc
        runtime/dpdk.cc
        runtime/worker.cc
        runtime/config.cc
        rpc/pb/xlb.pb.cc
        rpc/server.cc
        rpc/control.cc
        conntrack/service.cc
        conntrack/table.cc
        conntrack/metric.cc
        conntrack/conn.cc
        utils/numa.cc
        utils/format.cc
        utils/iface.cc
        utils/time.cc
        headers/ip.cc
        headers/ether.cc
        ports/pmd.cc
        ports/kni.cc
        modules/ether_out.cc
        modules/ether_inc.cc
        modules/arp_inc.cc
        modules/ipv4_inc.cc
        modules/tcp_inc.cc
        modules/conntrack.cc
        )

link_directories(/usr/local/lib)
include_directories(${SRC_INCS} ${DPDK_INCS} ${SYS_INCS} 3rdparty)

add_definitions(-DNDEBUG)

#add_subdirectory(abseil-cpp)

add_executable(xlb xlb.cc ${OBJS})
#add_executable(timer_test tests/timer.cc ${OBJS})
#add_executable(x_map_test tests/x_map.cc ${OBJS})

set_property(TARGET xlb PROPERTY INTERPROCEDURAL_OPTIMIZATION True)

target_link_libraries(xlb ${DPDK_LIBS} ${3RD_PARTY_LIBS} ${SYS_LIBS})
#target_link_libraries(x_map_test ${DPDK_LIBS} ${3RD_PARTY_LIBS} ${SYS_LIBS} ${GTEST_LIBS})
