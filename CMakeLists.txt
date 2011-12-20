cmake_minimum_required(VERSION 2.8)

project(Packer C)

option(ROTATE_PIECES "Use rotated pieces [experimental]" OFF)
option(SORT_PIECES   "Sort pieces in increasing size"    ON)
option(USE_THREADS   "Support for pthreads"              OFF)
option(BUILD_TESTING "Include the testing harness"       OFF)

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
    set(HW6_LIBRARIES ${PACKER_LIBRARIES} ${THREAD_LIBRARY_PATH})
  else(THREAD_LIBRARY_PATH)
    message(FATAL_ERROR "Cannot find thread library")
  endif(THREAD_LIBRARY_PATH)
endif(USE_THREADS)

add_executable(${CMAKE_PROJECT_NAME} pack.c)
set(CURRENT_EXECUTABLES ${CMAKE_PROJECT_NAME} ${CURRENT_EXECUTABLES})
target_link_libraries(${CMAKE_PROJECT_NAME} ${PACKER_LIBRARIES})

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
install(
  TARGETS ${CURRENT_EXECUTABLES}
  RUNTIME DESTINATION "${EXECUTABLE_OUTPUT_PATH}"
)

if(BUILD_TESTING)
  include(CTest)
  set(CTEST_PROJECT_NAME ${CMAKE_PROJECT_NAME})

  set(TEST_FILES
    puzzle1
    puzzle2
    puzzle3
    puzzle4
    puzzle5
    puzzle6)

  foreach(TEST_FILE ${TEST_FILES})
    add_test(${TEST_FILE} "${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}" "${PROJECT_SOURCE_DIR}/Testing/${TEST_FILE}.txt")
  endforeach(TEST_FILE)
endif(BUILD_TESTING)
