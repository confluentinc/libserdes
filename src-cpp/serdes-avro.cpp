/**
 * Copyright 2015-2023 Confluent Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <exception>

#include <avro/Compiler.hh>
#include <avro/Encoder.hh>
#include <avro/Decoder.hh>
#include <avro/Generic.hh>
#include <avro/Specific.hh>
#include <avro/Exception.hh>

#include "serdescpp-avro_int.h"


namespace Serdes {

static void *schema_load_cb_trampoline (serdes_schema_t *schema,
                                        const char *definition,
                                        size_t definition_len,
                                        char *errstr, size_t errstr_size,
                                        void *opaque) {
  avro::ValidSchema *avro_schema = new(avro::ValidSchema);

  try {
    *avro_schema = avro::compileJsonSchemaFromMemory((const uint8_t *)definition,
                                                    definition_len);
  } catch (const avro::Exception &e) {
    snprintf(errstr, errstr_size, "Failed to compile JSON schema: %s",
             e.what());
    delete avro_schema;
    return NULL;
  }

  return avro_schema;
}

static void schema_unload_cb_trampoline (serdes_schema_t *schema,
                                         void *schema_obj, void *opaque) {
  avro::ValidSchema *avro_schema = static_cast<avro::ValidSchema*>(schema_obj);
  delete avro_schema;
}

Serdes::Avro::~Avro () {

}

Avro *Avro::create (const Conf *conf, std::string &errstr) {
  AvroImpl *avimpl = new AvroImpl();

  if (create_serdes(avimpl, conf, errstr, schema_load_cb_trampoline, schema_unload_cb_trampoline) == -1) {
    delete avimpl;
    return NULL;
  }

  return avimpl;
}


ssize_t AvroImpl::serialize (Schema *schema, const avro::GenericDatum *datum,
                             std::vector<char> &out, std::string &errstr) {
  auto avro_schema = schema->object();

  /* Binary encoded output stream */
  auto bin_os = avro::memoryOutputStream();
  /* Avro binary encoder */
  auto bin_encoder = avro::validatingEncoder(*avro_schema, avro::binaryEncoder());

  try {
    /* Encode Avro datum to Avro binary format */
    bin_encoder->init(*bin_os.get());
    avro::encode(*bin_encoder, *datum);
    bin_encoder->flush();

  } catch (const avro::Exception &e) {
    errstr = std::string("Avro serialization failed: ") + e.what();
    return -1;
  }

  /* Extract written bytes. */
  auto encoded = avro::snapshot(*bin_os.get());

  /* Write framing */
  schema->framing_write(out);

  /* Write binary encoded Avro to output vector */
  out.insert(out.end(), encoded->cbegin(), encoded->cend());

  return out.size();
}


ssize_t AvroImpl::deserialize (Schema **schemap, avro::GenericDatum **datump,
                               const void *payload, size_t size,
                               std::string &errstr) {
  serdes_schema_t *ss;

  /* Read framing */
  char c_errstr[256];
  ssize_t r = serdes_framing_read(sd_, &payload, &size, &ss,
                                  c_errstr, sizeof(c_errstr));
  if (r == -1) {
    errstr = c_errstr;
    return -1;
  } else if (r == 0 && !*schemap) {
    errstr = "Unable to decode payload: No framing and no schema specified";
    return -1;
  }

  Schema *schema = *schemap;
  if (!schema) {
    schema = Serdes::Schema::get(dynamic_cast<HandleImpl*>(this),
                                 serdes_schema_id(ss), errstr);
    if (!schema)
      return -1;
  }

  avro::ValidSchema *avro_schema = schema->object();

  /* Binary input stream */
  auto bin_is = avro::memoryInputStream((const uint8_t *)payload, size);

  /* Binary Avro decoder */
  avro::DecoderPtr bin_decoder = avro::validatingDecoder(*avro_schema,
                                                         avro::binaryDecoder());

  avro::GenericDatum *datum = new avro::GenericDatum(*avro_schema);

  try {
    /* Decode binary to Avro datum */
    bin_decoder->init(*bin_is);
    avro::decode(*bin_decoder, *datum);

  } catch (const avro::Exception &e) {
    errstr = std::string("Avro deserialization failed: ") + e.what();
    delete datum;
    return -1;
  }

  *schemap = schema;
  *datump = datum;
  return 0;
}

}

