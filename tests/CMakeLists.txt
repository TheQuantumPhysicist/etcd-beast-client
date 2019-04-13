include_directories(../include)

add_executable(etcd-beast-tests
    test_general.cpp
    ${GTEST_PATH}/src/gtest_main.cc
    )

target_link_libraries(etcd-beast-tests
    etcd-beast
    gtest
    )

add_test(
    NAME etcd-beast-tests
    COMMAND etcd-beast-tests
    --datadir ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
