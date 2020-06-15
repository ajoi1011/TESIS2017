#
# Makefile
#
# Make file for 
#


PROG		= mcu
SOURCES		= main.cxx custom.cxx html.cxx conference.cxx video.cxx template.cxx  

OPAL_MAKE_DIR := $(if $(OPALDIR),$(OPALDIR)/make,$(shell pkg-config opal --variable=makedir))
ifeq ($(OPAL_MAKE_DIR),)
  $(error Cannot build without OPAL installed or OPALDIR set)
endif
include $(OPAL_MAKE_DIR)/opal.mak
