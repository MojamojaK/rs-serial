cmake_minimum_required(VERSION 3.9.0)
set(CMAKE_CXX_STANDARD 17)
project(rs-serial VERSION 1.0.0 DESCRIPTION "rs-serial")

SET(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")

add_subdirectory(tools)
add_subdirectory(third-party)
