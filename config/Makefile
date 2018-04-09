# Dynamic Link Exchange Protocol (DLEP)
#
# Copyright (C) 2015, 2016, 2017 Massachusetts Institute of Technology

include ../defs.mk

# files that can be validated against protocol-config.xsd
DRAFTS_XML = protocol/dlep-draft-24.xml \
             protocol/dlep-draft-29.xml \
             protocol/dlep-rfc-8175.xml \


# all files to be installed
INSTALL_FILES = modem.xml router.xml \
                protocol/protocol-config.xsd \
                protocol/credit-windowing-03.xml \
                protocol/latency-range-03.xml \
                $(DRAFTS_XML)

all: wellformedxml validxml

wellformedxml:
	@for f in $(INSTALL_FILES); do \
	    xmllint -noout $$f && echo "well-formed xml: $$f"; \
	done

validxml:
	@for f in $(DRAFTS_XML); do \
	    xmllint --noout --xinclude --schema protocol/protocol-config.xsd $$f; \
	done
install::
	install -d -v $(INSTALL_CONFIG)
	install -m a+r $(INSTALL_FILES) $(INSTALL_CONFIG)

clean:

include ../epilog.mk
