project(elroy_common_msg)

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

# This CMake file allows users of generated code to simply add_subdirectory()
# to access this file, and it will handle code generation properly. It only
# runs code generation if needed, i.e. if any of the output files are missing
# or the input files have been modified. To include the generated code in your
# project, simply add elroy_common_msg to your target's target_link_libraries()


# This file contains a few variables of interest:
# INPUT_FILES: This is a list of files that the code generation system uses as
#              input. If any one of these files changes, it will re-trigger
#              code generation to ensure everything is still up to date.
# OUTPUT_FILES : This is a list of generated files that the code generation
#                process is expected to create. If any of these files are
#                missing, we re-trigger code generation.


# For INPUT_FILES, just glob anything in the scripts and src directories. If
# anything changes in there, chances are we want to re-generate code.
file(
  GLOB_RECURSE INPUT_FILES
  LIST_DIRECTORIES false
  ${CMAKE_CURRENT_LIST_DIR}/scripts/**
  ${CMAKE_CURRENT_LIST_DIR}/src/**)

# For OUTPUT_FILES, we need to be more explicit. Call a script to generate the
# list of output files for us.
execute_process(
  COMMAND python3 get_output_file_list_from_manifest.py
  OUTPUT_VARIABLE OUTPUT_FILES
  RESULT_VARIABLE RETURN_VALUE
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/scripts)
if (NOT RETURN_VALUE EQUAL 0)
    message(FATAL_ERROR "Failed to run get_output_file_list_from_manifest.py")
endif()

# Because reasons, we need to create a custom target that depends on the output
# files so that the custom command will actually run.
add_custom_target(elroy_common_msg_gen ALL DEPENDS ${OUTPUT_FILES})
add_custom_command(
  COMMAND ${CMAKE_CURRENT_LIST_DIR}/scripts/generate_msg_code.sh
  DEPENDS ${INPUT_FILES}
  OUTPUT ${OUTPUT_FILES}
  COMMENT "Generating elroy_common_msg messages..."
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

# Create a library target that other cmake targets can depend on
add_library(elroy_common_msg INTERFACE)

# Make this target depend on the message generation target
add_dependencies(elroy_common_msg elroy_common_msg_gen)

target_include_directories(elroy_common_msg INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/generated/cpp/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/private>
    $<INSTALL_INTERFACE:generated/cpp/include>
    $<INSTALL_INTERFACE:private>)

option(ENABLE_TESTS "Enable tests" OFF)
if (${ENABLE_TESTS})
    add_subdirectory(test)
endif()
