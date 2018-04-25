# files that can be validated against protocol-config.xsd
file(GLOB DRAFTS_XML RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/protocol/dlep*.xml)

# gather all xml/xsd files
file(GLOB XML_INSTALL_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/protocol/*.x[ms][ld])

foreach ( file ${XML_INSTALL_FILES} )
  add_test(wellformedxml_${file} ${LIBXML2_XMLLINT_EXECUTABLE} --noout ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach(file)

foreach ( file ${DRAFTS_XML} )
  add_test(validatedxml_${file} ${LIBXML2_XMLLINT_EXECUTABLE} --noout --xinclude --schema ${CMAKE_CURRENT_SOURCE_DIR}/protocol/protocol-config.xsd ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach(file)

install(FILES ${XML_INSTALL_FILES} DESTINATION ${ETCDIR}/dlep)
