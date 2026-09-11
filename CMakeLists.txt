cmake_minimum_required(VERSION 3.21)
project(IconChanceRandomizer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Geode REQUIRED)

add_geode_mod(${PROJECT_NAME}
    src/main.cpp
)
