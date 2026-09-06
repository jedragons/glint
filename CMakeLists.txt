cmake_minimum_required(VERSION 3.14)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Glint
    VERSION 0.1.0
    LANGUAGES CXX C
)

add_subdirectory(./examples/)
add_subdirectory(./sources/)
add_subdirectory(./vendor/)
