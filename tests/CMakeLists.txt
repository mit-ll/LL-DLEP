SET(TEST_SRCS serialize.cpp dataitems.cpp messages.cpp lib_interface.cpp dest_advert.cpp LogFile.cpp TestClientImpl.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -Wfatal-errors")

find_package(Boost REQUIRED COMPONENTS unit_test_framework)
add_definitions(${BOOST_INCREASE_VARIANT_TYPES})
add_definitions(-DBOOST_TEST_DYN_LINK)

include_directories(${LIBXML2_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(lib_tests ${TEST_SRCS})
target_link_libraries(lib_tests dlepShared ${LIBXML2_LIBRARIES} ${PROTOBUF_LIBRARIES} ${Boost_LIBRARIES})

add_test(NAME BoostTests COMMAND lib_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
