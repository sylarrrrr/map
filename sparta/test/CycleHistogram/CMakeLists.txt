project(CycleHistogram_test)

include(${SPARTA_CMAKE_MACRO_PATH}/SpartaTestingMacros.cmake)

sparta_add_test_executable(CycleHistogram_test CycleHistogram_test.cpp)
sparta_test(CycleHistogram_test CycleHistogram_test_RUN)
