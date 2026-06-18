add_executable(test_invariants test_invariants.cpp)
target_link_libraries(test_invariants PRIVATE dune_daq_codec)
add_test(NAME invariants COMMAND test_invariants)

add_executable(test_decode test_decode.cpp)
target_link_libraries(test_decode PRIVATE dune_daq_codec)
add_test(NAME decode COMMAND test_decode)
