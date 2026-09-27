add_library(Bite-bencode
    bencode.cpp
    bencode.hpp
)

target_include_directories(Bite-bencode PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
