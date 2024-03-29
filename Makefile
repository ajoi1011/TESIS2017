#
# Makefile
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is Open Phone Abstraction Library.
#
# The Initial Developer of the Original Code is Post Increment
#
#


PROG		= opaleie
SOURCES		= custom.cxx call.cxx html.cxx  managers.cxx main.cxx  h323.cxx conference.cxx video.cxx 

OPAL_MAKE_DIR := $(if $(OPALDIR),$(OPALDIR)/make,$(shell pkg-config opal --variable=makedir))
ifeq ($(OPAL_MAKE_DIR),)
  $(error Cannot build without OPAL installed or OPALDIR set)
endif
include $(OPAL_MAKE_DIR)/opal.mak


# End of Makefile
