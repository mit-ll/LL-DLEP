find_package(Doxygen REQUIRED)
if(NOT ${DOXYGEN_DOT_FOUND})
  message(FATAL_ERROR "The Dot program was not found. Please install Graphviz to provide this executable.")
endif(NOT ${DOXYGEN_DOT_FOUND})

configure_file(doxygen.config.in ${CMAKE_CURRENT_BINARY_DIR}/doxygen.config @ONLY)
add_custom_target(doc ALL ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doxygen.config
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doxygen.config
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating documentation with doxygen")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen/ DESTINATION ${DOCDIR} PATTERN "*.md5" EXCLUDE)
