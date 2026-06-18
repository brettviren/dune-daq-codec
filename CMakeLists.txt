cmake_minimum_required(VERSION 3.25)
project(dune_daq_codec VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wpedantic -Werror)

# Pure, dependency-free DUNE DAQ ADC codec. For now: the data-driven format
# descriptor + version-robustness invariants (ddm-3j8.1.7.3); the generic decode
# loop lands with ddm-3j8.1.1. Header-only so far; depends only on dune-daq-types.
find_package(dune_daq_types CONFIG REQUIRED)

add_library(dune_daq_codec INTERFACE)
target_include_directories(dune_daq_codec INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(dune_daq_codec INTERFACE dune_daq_types::dune_daq_types)
target_compile_features(dune_daq_codec INTERFACE cxx_std_20)

enable_testing()
add_subdirectory(test)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS dune_daq_codec EXPORT dune_daq_codecTargets)
install(DIRECTORY dune_daq_codec/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dune_daq_codec"
  FILES_MATCHING PATTERN "*.hpp")
install(EXPORT dune_daq_codecTargets
  FILE dune_daq_codecTargets.cmake
  NAMESPACE dune_daq_codec::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dune_daq_codec")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/dune_daq_codecConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dune_daq_codecConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/dune_daq_codecConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dune_daq_codec")
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/dune_daq_codecConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/dune_daq_codecConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dune_daq_codec")
