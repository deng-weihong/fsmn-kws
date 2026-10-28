cmake_minimum_required(VERSION 3.16)
project(fsmn_kws_consumer LANGUAGES CXX)
find_package(fsmn_kws 0.1 CONFIG REQUIRED)
add_executable(consumer main.cpp)
set_target_properties(consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(consumer PRIVATE fsmn_kws::fsmn_kws)
