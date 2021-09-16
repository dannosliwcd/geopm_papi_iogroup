#  Copyright (c) 2020, Intel Corporation
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#      * Redistributions of source code must retain the above copyright
#        notice, this list of conditions and the following disclaimer.
#
#      * Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in
#        the documentation and/or other materials provided with the
#        distribution.
#
#      * Neither the name of Intel Corporation nor the names of its
#        contributors may be used to endorse or promote products derived
#        from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
PAPI_LIB_DIR = ${HOME}/.local/lib
PAPI_INC_DIR = ${HOME}/.local/include

GEOPM_INC_DIR = ${HOME}/build/geopm/include

LDLIBS += -L$(PAPI_LIB_DIR) -lpapi -Wl,-rpath=$(PAPI_LIB_DIR)
CPPFLAGS += -I$(PAPI_INC_DIR) -I$(GEOPM_INC_DIR) -Wall -Werror -fPIC
CXXFLAGS = -g -std=c++11
LINK.o = $(LINK.cc)

libgeopmiogroup_papi_iogroup.so.0.0.0: papi_iogroup.o
	$(LINK.cc) $(LOADLIBES) $(LDLIBS) -shared $< $(OUTPUT_OPTION)

papi_iogroup.o: papi_iogroup.cpp
	$(COMPILE.cc) $< $(OUTPUT_OPTION)

.PHONY: install
install: libgeopmiogroup_papi_iogroup.so.0.0.0
	@test "${GEOPM_PLUGIN_PATH}" || ( echo "Error: GEOPM_PLUGIN_PATH must be set before installing this plugin"; exit 1 )
	cp libgeopmiogroup_papi_iogroup.so.0.0.0 "${GEOPM_PLUGIN_PATH}"

.PHONY: clean
clean:
	rm -f libgeopmiogroup_papi_iogroup.so* papi_iogroup.o
