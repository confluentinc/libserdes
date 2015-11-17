
include ../Makefile.config

EXAMPLES_$(ENABLE_AVRO_C) += serdes-kafka-avro-client
EXAMPLES_$(ENABLE_AVRO_CPP) += serdes-tool
EXAMPLES ?= $(EXAMPLES_y)

all: $(EXAMPLES)

include ../mklove/Makefile.base

CFLAGS += -I../src
CXXFLAGS += -I../src -I../src-cpp

SLIB=../src/libserdes.a
SLIB_CPP=../src-cpp/libserdes++.a

# lib must be compiled with -gstrict-dwarf, but examples must not,
# due to some clang bug on OSX 10.9
CPPFLAGS := $(subst strict-dwarf,,$(CPPFLAGS))

serdes-kafka-avro-client: $(SLIB) serdes-kafka-avro-client.c
	$(CC) $(CPPFLAGS) $(CFLAGS) serdes-kafka-avro-client.c \
	-o $@ $(LDFLAGS) $(SLIB) $(LIBS) -lrdkafka -lrt

serdes-tool: $(SLIB) $(SLIB_CPP) serdes-tool.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ \
	-o $@ $(LDFLAGS) $(SLIB_CPP) $(SLIB) $(LIBS)

clean:
	rm -f $(EXAMPLES)

