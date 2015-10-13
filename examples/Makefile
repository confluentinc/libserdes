EXAMPLES ?= serdes-kafka-avro-client

all: $(EXAMPLES)

include ../mklove/Makefile.base

CFLAGS += -I../src
CXXFLAGS += -I../src-cpp

SLIB=../src/libserdes.a

# lib must be compiled with -gstrict-dwarf, but examples must not,
# due to some clang bug on OSX 10.9
CPPFLAGS := $(subst strict-dwarf,,$(CPPFLAGS))

serdes-kafka-avro-client: $(SLIB) serdes-kafka-avro-client.c
	$(CC) $(CPPFLAGS) $(CFLAGS) serdes-kafka-avro-client.c \
	-o $@ $(LDFLAGS) $(SLIB) $(LIBS) -lrdkafka -lrt
clean:
	rm -f $(EXAMPLES)

