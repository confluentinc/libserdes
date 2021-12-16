# libserdes

Copyright(C) 2015-2020 Confluent Inc.

 * This software is licensed under the Apache 2.0 license.
---------------------------------------------------------------------------

**libserdes** is a schema-based serializer/deserializer C/C++ library with
support for [Avro](http://avro.apache.org) and the
[Confluent Platform Schema Registry](https://github.com/confluentinc/schema-registry).

The library is aimed to be used in the streaming pipeline,
e.g. [Apache Kafka](http://kafka.apache.org), to perform data serialization
and deserialization with centrally managed schemas.

On the producer side the application only needs to provide the centrally
managed schema name or id and libserdes will fetch and load the schema from
the remote schema registry. The loaded schema will be used for producer side
verification and serialization of Avro objects.

On the consumer side libserdes will automatically extract the schema id
of consumed serialized data and fetch the required schemas from the
remote schema registry to perform verification and deserialization back
to an Avro object.

This functionality is designed to work in close integration with the
Kafka consumer or producer, see the [examples](examples) directory for
integrations with the librdkafka client library.

The serialization defaults to Avro but is completely pluggable through the
configuration object interface, allowing applications to use libserdes for any
serialization type.

**libserdes** is licensed under Apache License 2.0


## Requirements

 * [jansson](http://www.digip.org/jansson/)
 * [libcurl](http://curl.haxx.se/)

On Debian/Ubuntu based systems:
`sudo apt-get install libjansson-dev libcurl-gnutls-dev`


## Build

    ./configure
    make
    sudo make install


### Documentation

Full API documentation is available in [serdes.h](src/serdes.h) and [serdescpp.h](src-cpp/serdescpp.h)

### Configuration

libserdes typically only needs to be configured with a list of URLs
to the schema registries, all other configuration is optional.

 * `schema.registry.url` - comma separated list of schema registry base URLs (no default)
 * `deserializer.framing` - expected framing format when deserializing data: `none` or `cp1` (Confluent Platform framing). (default: `cp1`)
 * `serializer.framing` - framing format inserted when serializing data: `none` or `cp1` (Confluent Platform framing). (default: `cp1`)
 * `debug` - enable/disable debugging with `all` or `none`. (default: `none`)
