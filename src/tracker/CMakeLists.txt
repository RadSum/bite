add_library(Bite-tracker
    announce.hpp
    response.hpp
    response.cpp
    url.hpp
    url.cpp
    trackerclient.hpp
    trackerclient.cpp
)
add_library(bite::tracker ALIAS Bite-tracker)

target_include_directories(Bite-tracker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Bite-tracker PUBLIC
    bite::core
    bite::bencode
    Boost::headers
    Boost::url
    OpenSSL::SSL
    OpenSSL::Crypto
)

add_executable(Bite-tracker-test
    main.cpp
)
target_link_libraries(Bite-tracker-test PRIVATE
    bite::tracker
    bite::torrent
)

if (BITE_BUILD_TESTS)
    add_executable(test-tracker
        response_test.cpp
        url_test.cpp
    )
    target_link_libraries(test-tracker PRIVATE
        bite::tracker
        Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(test-tracker)
endif()
