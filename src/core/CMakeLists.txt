add_library(Bite-core INTERFACE
    types.hpp
)
add_library(bite::core ALIAS Bite-core)

target_include_directories(Bite-core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
