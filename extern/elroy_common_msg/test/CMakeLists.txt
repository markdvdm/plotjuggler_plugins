# Create a unit test executable target
add_executable(
  elroy_common_msg_tests
  testmain.cpp msg_generation_ut.cc struct_generation_ut.cc
  ${PROJECT_SOURCE_DIR}/src/tests/unit_tests/msg_packing_util_ut.cc
  ${PROJECT_SOURCE_DIR}/generated/cpp/unit_tests/msg_handling_ut.cc)
set_source_files_properties(
  ${PROJECT_SOURCE_DIR}/generated/cpp/unit_tests/msg_handling_ut.cc
  PROPERTIES GENERATED 1)
target_link_libraries(elroy_common_msg_tests elroy_common_msg)
target_include_directories(
  elroy_common_msg_tests
  PRIVATE
  private)
target_compile_options(elroy_common_msg_tests PRIVATE -Wall -Werror -Wextra
                                                      -Wpedantic)
target_compile_features(elroy_common_msg_tests PRIVATE cxx_std_17)

include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
find_package(GTest)
if(TARGET GTest::gtest)
  target_link_libraries(elroy_common_msg_tests GTest::gtest)
else()
  message(FATAL_ERROR "Could not find gtest library!")
endif()
