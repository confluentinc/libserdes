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
#pragma once

/**
 * Internal Avro implementation of libserdes C++ interface
 */

#include "serdescpp_int.h"
#include "serdescpp-avro.h"

namespace Serdes {


  class AvroImpl : public Avro, public HandleImpl {
 public:
    ~AvroImpl () { }

    static Avro *create (const Conf *conf, std::string &errstr);

    ssize_t serialize (Schema *schema, const avro::GenericDatum *datum,
                       std::vector<char> &out, std::string &errstr);

    ssize_t deserialize (Schema **schemap, avro::GenericDatum **datump,
                         const void *payload, size_t size, std::string &errstr);

    ssize_t serializer_framing_size () const {
      return HandleImpl::serializer_framing_size();
    }

    ssize_t deserializer_framing_size () const {
      return HandleImpl::deserializer_framing_size();
    }

    int schemas_purge (int max_age) {
      return HandleImpl::schemas_purge(max_age);
    }

  };

}
