/**
 * Copyright 2015 Confluent Inc.
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
#pragma once

#include <string>

#include <avro/ValidSchema.hh>

#include "serdescpp.h"

namespace Serdes {

class SERDES_EXPORT Avro : public virtual Handle {
public:
  virtual ~Avro () = 0;

  /**
   * Create Avro serializer/deserializer
   */
  static Avro *create (const Conf *conf, std::string &errstr);

  /**
   * Serialize the generic Avro datum to output vector 'out'.
   * Returns the number of bytes written to 'out' or -1 on error (in which
   * case errstr is set).
   */
  virtual ssize_t serialize (Schema *schema, const avro::GenericDatum *datum,
                             std::vector<char> &out, std::string &errstr) = 0;

  /**
   * Deserialize binary buffer `payload` of size `size` to generic Avro datum.
   * If '*schemap' is NULL the payload is expected to have the same framing
   * as configured `deserializer.framing` and the deserializer will use
   * the framing to look up the schema used and also return the schema
   * in `*schemap`.
   *
   * If `*schemap` is non-NULL no framing is expected and the deserializer
   * will use the provided schema to deserialize the payload.
   *
   * Returns the number of bytes read from `payload` on success, else
   * returns -1 and sets `errstr` accordingly.
   */
  virtual ssize_t deserialize (Schema **schemap, avro::GenericDatum **datump,
                               const void *payload, size_t size,
                               std::string &errstr) = 0;
};

}
