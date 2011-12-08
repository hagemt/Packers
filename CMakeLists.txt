cmake_minimum_required(VERSION 2.8)

project(HW6 C)

option(VERBOSE_MODE  "Produce verbose output"            OFF)
option(ROTATE_PIECES "Use rotated pieces [experimental]" OFF)
option(SORT_PIECES   "Sort pieces in increasing size"    ON)
option(USE_THREADS   "Support for pthreads"              OFF)
option(BUILD_TESTING "Include the testing harness"       OFF)

if(VERBOSE_MODE)
  set(CMAKE_C_FLAGS "-DVERBOSE ${CMAKE_C_FLAGS}")
endif(VERBOSE_MODE)

if(ROTATE_PIECES)
  set(CMAKE_C_FLAGS "-DROTATIONS ${CMAKE_C_FLAGS}")
endif(ROTATE_PIECES)

if(SORT_PIECES)
  set(CMAKE_C_FLAGS "-DSORT ${CMAKE_C_FLAGS}")
endif(SORT_PIECES)

if(USE_THREADS)
  find_library(THREAD_LIBRARY_PATH pthread)
  if(THREAD_LIBRARY_PATH)
    set(CMAKE_C_FLAGS "-DTHREADS ${CMAKE_C_FLAGS}")
    set(HW6_LIBRARIES ${HW6_LIBRARIES} ${THREAD_LIBRARY_PATH})
  else(THREAD_LIBRARY_PATH)
    message(FATAL_ERROR "Cannot find thread library")
  endif(THREAD_LIBRARY_PATH)
endif(USE_THREADS)

add_executable(hw6 hw6.c)
set(CURRENT_EXECUTABLES hw6 ${CURRENT_EXECUTABLES})
target_link_libraries(hw6 ${HW6_LIBRARIES})

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
install(
  TARGETS ${CURRENT_EXECUTABLES}
  RUNTIME DESTINATION "${EXECUTABLE_OUTPUT_PATH}"
)

if(BUILD_TESTING)
  include(CTest)
  set(CTEST_PROJECT_NAME ${CMAKE_PROJECT_NAME})
endif(BUILD_TESTING)
