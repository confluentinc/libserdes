
-include ../Makefile.config

EXAMPLES_$(ENABLE_AVRO_CPP) += serdes-tool
EXAMPLES_$(ENABLE_AVRO_C)$(ENABLE_LIBRDKAFKA) += serdes-kafka-avro-client
EXAMPLES_$(ENABLE_AVRO_CPP)$(ENABLE_LIBRDKAFKA) += kafka-serdes-avro-console-consumer
EXAMPLES_$(ENABLE_AVRO_CPP)$(ENABLE_LIBRDKAFKA) += kafka-serdes-avro-console-producer
EXAMPLES ?= $(EXAMPLES_y) $(EXAMPLES_yy)

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
	-o $@ $(LDFLAGS) $(SLIB) $(LIBS) -lrdkafka

serdes-tool: $(SLIB) $(SLIB_CPP) serdes-tool.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ \
	-o $@ $(LDFLAGS) $(SLIB_CPP) $(SLIB) $(LIBS)


kafka-serdes-avro-console-consumer: $(SLIB) $(SLIB_CPP) kafka-serdes-avro-console-consumer.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ \
	-o $@ $(LDFLAGS) $(SLIB_CPP) $(SLIB) $(LIBS) -lrdkafka++ -lrdkafka

kafka-serdes-avro-console-producer: $(SLIB) $(SLIB_CPP) kafka-serdes-avro-console-producer.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ \
	-o $@ $(LDFLAGS) $(SLIB_CPP) $(SLIB) $(LIBS) -lrdkafka++ -lrdkafka


clean:
	rm -f $(EXAMPLES)

