cmake_minimum_required(VERSION 3.14)

add_executable(${DUAL_TARGET} main.cpp client.cpp server.cpp tprint.cpp object.cpp)

if(NOT ${FAST_BUILD} EQUAL true)
    add_executable(${SERVER_TARGET} main.cpp server.cpp tprint.cpp object.cpp)
    target_compile_definitions(${SERVER_TARGET} PUBLIC -DNO_CLIENT)

    add_executable(${CLIENT_TARGET} main.cpp client.cpp tprint.cpp object.cpp)
    target_compile_definitions(${CLIENT_TARGET} PUBLIC -DNO_HOST)

endif()

