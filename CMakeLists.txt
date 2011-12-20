cmake_minimum_required(VERSION 2.8)

project(Packer C)

option(ROTATE_PIECES "Use rotated pieces [experimental]" OFF)
option(SORT_PIECES   "Sort pieces in increasing size"    ON)
option(USE_THREADS   "Support for pthreads"              OFF)

if(ROTATE_PIECES)
  set(CMAKE_C_FLAGS "-DROTATIONS ${CMAKE_C_FLAGS}")
endif(ROTATE_PIECES)

if(SORT_PIECES)
  set(CMAKE_C_FLAGS "-DSORT ${CMAKE_C_FLAGS}")
endif(SORT_PIECES)

if(USE_THREADS)
  if(ROTATE_PIECES)
    message(WARNING "Piece rotation is not thread-safe!")
  endif(ROTATE_PIECES)
  find_library(THREAD_LIBRARY_PATH pthread)
  if(THREAD_LIBRARY_PATH)
    set(CMAKE_C_FLAGS "-DTHREADS ${CMAKE_C_FLAGS}")
    set(CURRENT_LIBRARIES ${CURRENT_LIBRARIES} ${THREAD_LIBRARY_PATH})
  else(THREAD_LIBRARY_PATH)
    message(FATAL_ERROR "Cannot find thread library")
  endif(THREAD_LIBRARY_PATH)
endif(USE_THREADS)

add_executable(${CMAKE_PROJECT_NAME} pack.c)
set(CURRENT_EXECUTABLES ${CMAKE_PROJECT_NAME} ${CURRENT_EXECUTABLES})
target_link_libraries(${CMAKE_PROJECT_NAME} ${CURRENT_LIBRARIES})

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
install(
  TARGETS ${CURRENT_EXECUTABLES}
  RUNTIME DESTINATION "${EXECUTABLE_OUTPUT_PATH}"
)

include(CTest)
if(BUILD_TESTING)
  set(CTEST_PROJECT_NAME ${CMAKE_PROJECT_NAME})
  set(DART_TESTING_TIMEOUT 15)
  set(TEST_EXECUTABLE "${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}")
  set(TEST_FILE_DIR "${PROJECT_SOURCE_DIR}/Testing")

  add_test(NAME usage_string COMMAND "${TEST_EXECUTABLE}")
  set_tests_properties(usage_string
    PROPERTIES PASS_REGULAR_EXPRESSION "USAGE:.*")

  function(add_sample_test ID RESULT)
    add_test(
      NAME sample_${ID}
      COMMAND "${TEST_EXECUTABLE}"
              "${TEST_FILE_DIR}/Samples/${ID}.txt")
    if(RESULT)
      set_tests_properties(sample_${ID}
        PROPERTIES PASS_REGULAR_EXPRESSION "${RESULT} solutions? found:")
    else(RESULT)
      set_tests_properties(sample_${ID}
        PROPERTIES PASS_REGULAR_EXPRESSION "No solutions found.")
    endif(RESULT)
  endfunction(add_sample_test)

  add_sample_test(puzzle1   16)
  add_sample_test(puzzle2    0)
  add_sample_test(puzzle3    8)
  add_sample_test(puzzle4    8)
  add_sample_test(puzzle5 4096)
  add_sample_test(puzzle6    8)
endif(BUILD_TESTING)
