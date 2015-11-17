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

/**
 * Internal implementation of libserdes C++ interface
 */

#include <cstdarg>

extern "C" {
#include "serdes.h"
};


namespace Serdes {


  std::string err2str (ErrorCode err) {
    return std::string(serdes_err2str(static_cast<serdes_err_t>(err)));
  }



  class ConfImpl : public Conf {
 public:
    ~ConfImpl () {
      if (conf_)
        serdes_conf_destroy(conf_);
    }

    static Conf *create ();

    ConfImpl (): conf_(NULL), log_cb_(NULL) { }



    ErrorCode set (const std::string &name,
                   const std::string &value,
                   std::string &errstr) {
      char c_errstr[256];
      serdes_err_t err;
      err = serdes_conf_set(conf_, name.c_str(), value.c_str(),
                            c_errstr, sizeof(c_errstr));
      if (err != SERDES_ERR_OK)
        errstr = c_errstr;
      return static_cast<ErrorCode>(err);
    }

    void set (LogCb *log_cb) {
      log_cb_ = log_cb;
    }

    serdes_conf_t *conf_;
    LogCb *log_cb_;

  };


  class HandleImpl : public Handle {
 public:
    ~HandleImpl () {
      if (sd_)
        serdes_destroy(sd_);
    }

    static Handle *create (const Conf *conf, std::string &errstr);

    HandleImpl (): log_cb_(NULL), sd_(NULL) {}

    int schemas_purge (int max_age) {
      return serdes_schemas_purge(sd_, max_age);
    }

    ssize_t serializer_framing_size () const {
      return serdes_serializer_framing_size(sd_);
    }

    ssize_t deserializer_framing_size () const {
      return serdes_deserializer_framing_size(sd_);
    }



    LogCb *log_cb_;
    serdes_t *sd_;
  };


  class SchemaImpl : public Schema {
 public:
    ~SchemaImpl () {
      if (schema_) {
        serdes_schema_set_opaque(schema_, NULL);
        serdes_schema_destroy(schema_);
      }
    }

    SchemaImpl (): schema_(NULL) {}
    SchemaImpl (serdes_schema_t *ss): schema_(ss) {}

    static Schema *get (Handle *handle, int id, std::string &errstr);
    static Schema *get (Handle *handle, std::string &name, std::string &errstr);

    static Schema *add (Handle *handle, int id, std::string &definition,
                        std::string &errstr);
    static Schema *add (Handle *handle, std::string name, std::string &definition,
                        std::string &errstr);
    static Schema *add (Handle *handle, int id, std::string name,
                        std::string &definition, std::string &errstr);


    int id () {
      return serdes_schema_id(schema_);
    }

    const std::string name () {
      const char *name = serdes_schema_name(schema_);
      return std::string(name ? name : "");
    }

    const std::string definition () {
      const char *def = serdes_schema_definition(schema_);
      return std::string(def ? def : "");
    }

    avro::ValidSchema *object () {
      return static_cast<avro::ValidSchema*>(serdes_schema_object(schema_));
    }


    ssize_t framing_write (std::vector<char> &out) const {
      ssize_t framing_size = serdes_serializer_framing_size(serdes_schema_handle(schema_));
      if (framing_size == 0)
        return 0;

      /* Make room for framing */
      int pos = out.size();
      out.resize(out.size() + framing_size);

      /* Write framing */
      return serdes_framing_write(schema_, &out[pos], framing_size);
    }

    serdes_schema_t *schema_;
  };


  class AvroImpl : virtual public Avro, virtual public HandleImpl {
 public:
    ~AvroImpl () { }

    static Avro *create (const Conf *conf, std::string &errstr);

    ssize_t serialize (Schema *schema, const avro::GenericDatum *datum,
                       std::vector<char> &out, std::string &errstr);

    ssize_t deserialize (Schema **schemap, avro::GenericDatum **datump,
                         const void *payload, size_t size, std::string &errstr);

    ssize_t serializer_framing_size () const {
      return dynamic_cast<const HandleImpl*>(this)->serializer_framing_size();
    }

    ssize_t deserializer_framing_size () const {
      return dynamic_cast<const HandleImpl*>(this)->deserializer_framing_size();
    }

    int schemas_purge (int max_age) {
      return dynamic_cast<HandleImpl*>(this)->schemas_purge(max_age);
    }

  };

}
