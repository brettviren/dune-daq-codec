add_executable(test_invariants test_invariants.cpp)
target_link_libraries(test_invariants PRIVATE dune_daq_codec)
add_test(NAME invariants COMMAND test_invariants)

add_executable(test_decode test_decode.cpp)
target_link_libraries(test_decode PRIVATE dune_daq_codec)
add_test(NAME decode COMMAND test_decode)

# Golden validation against real DUNE data (committed fixtures; reference-free).
add_executable(test_golden test_golden.cpp)
target_link_libraries(test_golden PRIVATE dune_daq_codec)
target_compile_definitions(test_golden PRIVATE
  FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME golden COMMAND test_golden)
