add_library(Bite-tracker
    trackerclient.cpp
    trackerclient.hpp
)
add_library(bite::tracker ALIAS Bite-tracker)

target_include_directories(Bite-tracker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Bite-tracker PUBLIC
    Boost::headers
)
