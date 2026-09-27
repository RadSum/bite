add_library(Bite-bencode
    bencode.cpp
    bencode.hpp
)

target_include_directories(Bite-bencode PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (BITE_BUILD_TESTS)
    add_executable(test-bencode bencode_test.cpp)
    target_link_libraries(test-bencode PRIVATE
        Bite-bencode
        Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(test-bencode)
endif()
