project(SIMDB_Thread_test)

add_executable(SIMDB_Thread_test Thread_test.cpp)

include(${SPARTA_CMAKE_MACRO_PATH}/SimdbTestingMacros.cmake)

simdb_test(SIMDB_Thread_test SIMDB_Thread_test_RUN)

add_subdirectory(StandaloneCpp1)
