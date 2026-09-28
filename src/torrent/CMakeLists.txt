add_library(Bite-torrent
    metainfo.hpp
    metainfo.cpp
)
add_library(bite::torrent ALIAS Bite-torrent)

target_include_directories(Bite-torrent PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Bite-torrent PUBLIC
    bite::bencode
    OpenSSL::Crypto
)

add_executable(Bite-torrent-test
    main.cpp
)
target_link_libraries(Bite-torrent-test PRIVATE
    bite::torrent
)
