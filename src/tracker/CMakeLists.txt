add_library(Bite-tracker
    announce.hpp
    response.hpp
    response.cpp
)
add_library(bite::tracker ALIAS Bite-tracker)

target_include_directories(Bite-tracker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Bite-tracker PUBLIC
    bite::core
    bite::bencode
    Boost::headers
)
