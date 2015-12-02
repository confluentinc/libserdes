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


/**
 *
 * serdes-common.h contains artifacts shared between the C and C++ interface,
 * such as types and error codes.
 */
#include "serdes-common.h"


namespace Serdes {

SERDES_EXPORT
int      version ();


SERDES_EXPORT
std::string  version_str ();


typedef serdes_err_t ErrorCode;



/**
 * Returns the human readable form of a serdes_err_t
 * The returned pointer has infinite life time and must not be freed.
 */
SERDES_EXPORT
std::string err2str (Serdes::ErrorCode err);



/* Forward declarations */
class Handle;

/**
 * Set optional log callback to use for serdes originated log messages.
 */
class SERDES_EXPORT LogCb {
public:
  virtual void log_cb (Handle *serdes, int level, const std::string &fac,
                       const std::string &buf) = 0;
};



/**
 * Reusable configuration object passed to Serdes::Handle::create()
 *
 */
class SERDES_EXPORT Conf {
public:

  /**
   * Create a configuration object with default parameters.
   * The object must be deleted when the application is done with it.
   */
  static Conf *create ();

  virtual ~Conf () {};

  /**
   * Set configuration property `name` to value `value`.
   * Returns SERDES_ERR_OK on success, else writes an error message to 'errstr'.
   */
  virtual ErrorCode set (const std::string &name,
                         const std::string &value,
                         std::string &errstr) = 0;

  /**
   * Set an optional log callback class for logs originating from libserdes.
   */
  virtual void set (LogCb *log_cb) = 0;
};




/**
 * Main Serdes handle.
 */
class SERDES_EXPORT Handle {
public:
  virtual ~Handle () {};

  /**
   * Create a new Serdes handle for serialization/deserialization.
   * `conf` is an optional configuration object.
   *
   * Returns a new Handle object, or NULL on error (see errstr for reason).
   */
  static Handle *create (const Conf *conf, std::string &errstr);


  /**
   * Purges any schemas from the local schema cache that have not been used
   * in `max_age` seconds.
   *
   * Returns the number of schemas removed.
   */
  virtual int schemas_purge (int max_age) = 0;

  /**
   * Returns the serializer/deserializer framing size,
   * or 0 if no framing is configured.
   */
  virtual ssize_t serializer_framing_size () const = 0;
  virtual ssize_t deserializer_framing_size () const = 0;
};



/**
 * A cached copy of a schema
 */
class SERDES_EXPORT Schema {
public:

  virtual ~Schema () {};

  /**
   * Get and load schema from local cache or remote schema registry.
   * The schema may be looked up by its `name` or by its `id`.
   *
   * The returned schema will be fully loaded and immediately usable.
   *
   * If the get or load fails NULL is returned and a human readable error
   * description is written to `errstr`.
   */
  static Schema *get (Handle *handle, int id, std::string &errstr);
  static Schema *get (Handle *handle, const std::string &name,
                      std::string &errstr);

  /**
   * Add schema definition to the local cache and stores the schema to remote
   * schema registry.
   *
   * The schema `name` is required, but the `id` is optional.
   * If `id` is not set then the schema will be stored on the remote schema
   * registry, else the id will be assigned as this schema's id.
   *
   * If an existing schema with an identical schema exists in the cache it
   * will be returned instead, else the newly created schema will be returned.
   *
   * The returned schema will be fully loaded and immediately usable.
   *
   * In case schema parsing or storing fails NULL is returned and a human
   * readable error description is written to `errstr`
 */
  static Schema *add (Handle *handle, int id,
                      const std::string &definition, std::string &errstr);
  static Schema *add (Handle *handle, const std::string &name,
                      const std::string &definition, std::string &errstr);
  static Schema *add (Handle *handle, const std::string &name, int id,
                      const std::string &definition, std::string &errstr);


  /**
   * Returns the schema id.
   */
  virtual int id () = 0;

  /**
   * Returns the schema name.
   * The returned pointer is only valid until the schema is destroyed.
   * NULL is returned if the name of the schema is not known.
   */
  virtual const std::string name () = 0;

  /**
   * Returns the schema definition.
   * The returned pointer is only valid until the schema is destroyed.
   */
  virtual const std::string definition () = 0;

  /**
   * Returns the Avro schema object.
   */
  virtual avro::ValidSchema *object () = 0;


  /**
   * Writes framing to vector.
   * Returns the number of bytes written.
   */
  virtual ssize_t framing_write (std::vector<char> &out) const = 0;
};


}
