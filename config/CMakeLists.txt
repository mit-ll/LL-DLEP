# files that can be validated against protocol-config.xsd
SET(DRAFTS_XML protocol/dlep-draft-08.xml 
             protocol/dlep-draft-17.xml 
             protocol/dlep-draft-19.xml 
             protocol/dlep-draft-21.xml 
             protocol/dlep-draft-24.xml
             protocol/dlep-draft-29.xml
             protocol/dlep-rfc-8175.xml)

SET(XML_INSTALL_FILES modem.xml router.xml
                protocol/protocol-config.xsd
                protocol/credit-windowing-00.xml
                protocol/credit-windowing-03.xml
                protocol/latency-range-03.xml
                ${DRAFTS_XML})

foreach ( file ${XML_INSTALL_FILES} )
  add_test(wellformedxml_${file} ${LIBXML2_XMLLINT_EXECUTABLE} --noout ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach(file)

foreach ( file ${DRAFTS_XML} )
  add_test(validatedxml_${file} ${LIBXML2_XMLLINT_EXECUTABLE} --noout --xinclude --schema ${CMAKE_CURRENT_SOURCE_DIR}/protocol/protocol-config.xsd ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach(file)

install(FILES ${XML_INSTALL_FILES} DESTINATION ${ETCDIR}/dlep)
