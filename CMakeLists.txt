project(etcd-beast)
cmake_minimum_required(VERSION 3.5.1)

#### enable thread sanitizer for clang
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize-blacklist=${CMAKE_SOURCE_DIR}/sanitizer-blacklist.txt")

# Download automatically, you can also just copy the conan.cmake file
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
   message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
   file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.14/conan.cmake"
                 "${CMAKE_BINARY_DIR}/conan.cmake")
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_run(CONANFILE conanfile.txt
                BASIC_SETUP
                BUILD missing
#                SETTINGS jsoncpp:compiler=gcc
                PROFILE_AUTO ALL # detects all cmake settings
                )

#include(${CMAKE_SOURCE_DIR}/cmake/FindBoost.cmake)
#set(Boost_NO_SYSTEM_PATHS ON)
#set(BOOST_ROOT "$ENV{HOME}/Programs/boost/boost/")
#set(BOOST_LIBRARYDIR "$ENV{HOME}/Programs/boost/boost/stage/lib/")
#set(BOOST_INCLUDEDIR "$ENV{HOME}/Programs/boost/boost/")

find_package(Boost 1.67 COMPONENTS program_options system thread random filesystem REQUIRED)
link_directories(${BOOST_LIBRARYDIR})
include_directories(${Boost_INCLUDE_DIRS})

###### GTEST ###############################################
# This is cloned from googletest repo
set(GTEST_PATH ${CMAKE_SOURCE_DIR}/3rdparty/googletest/googletest)
include_directories(${GTEST_PATH}/include)
include_directories(${GTEST_PATH}/)
add_library(gtest ${GTEST_PATH}/src/gtest-all.cc)
target_link_libraries(gtest -pthread)
############################################################

add_library(${PROJECT_NAME} STATIC
    ${CMAKE_SOURCE_DIR}/src/ETCDClient.cpp
    ${CMAKE_SOURCE_DIR}/src/HttpSession.cpp
    ${CMAKE_SOURCE_DIR}/src/ETCDResponse.cpp
    ${CMAKE_SOURCE_DIR}/src/ETCDError.cpp
    ${CMAKE_SOURCE_DIR}/src/JsonStringParserQueue.cpp
    ${CMAKE_SOURCE_DIR}/src/ETCDWatch.cpp
    ${CMAKE_SOURCE_DIR}/src/ETCDParsedResponse.cpp
    )

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(${PROJECT_NAME}
    -ljsoncpp
    -ldl
    -lpthread
    )

add_subdirectory(tests)

file(GLOB_RECURSE EtcdBeastHeaders
    "${CMAKE_SOURCE_DIR}/include/etcd-beast/*.h"
    )


install(FILES ${EtcdBeastHeaders}
    DESTINATION include/etcd-beast)

install(TARGETS ${PROJECT_NAME}
    DESTINATION lib)
