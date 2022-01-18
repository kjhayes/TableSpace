cmake_minimum_required(VERSION 3.14)

project(TableSpace)

set(FAST_BUILD false)

set(DUAL_TARGET ${CMAKE_PROJECT_NAME})

if(NOT ${FAST_BUILD} EQUAL true)
    set(SERVER_TARGET ${DUAL_TARGET}Server)
    set(CLIENT_TARGET ${DUAL_TARGET}Client)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

add_subdirectory(${CMAKE_SOURCE_DIR}/source)

target_include_directories(${DUAL_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(NOT ${FAST_BUILD} EQUAL true)
    target_include_directories(${SERVER_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(${CLIENT_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()

find_package(SFML COMPONENTS system network window graphics CONFIG REQUIRED)
target_link_libraries(${DUAL_TARGET} PRIVATE sfml-network sfml-graphics sfml-window sfml-system)

if(NOT ${FAST_BUILD} EQUAL true)
    target_link_libraries(${CLIENT_TARGET} PRIVATE sfml-network sfml-graphics sfml-window sfml-system)
    target_link_libraries(${SERVER_TARGET} PRIVATE sfml-system sfml-network sfml-graphics sfml-window)
endif()

file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})