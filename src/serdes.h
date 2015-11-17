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

#include "serdes-common.h"


/* Private types, all access through methods */
typedef struct serdes_s serdes_t;
typedef struct serdes_schema_s serdes_schema_t;
typedef struct serdes_conf_s serdes_conf_t;



/**
 * Supported serdes types
 */
typedef enum {
        SERDES_AVRO,
} serdes_type_t;



/*******************************************************************************
 *
 * Errors
 *
 ******************************************************************************/

/**
 * Error codes
 */
typedef enum {
        SERDES_ERR_OK = 0,            /* No error */
        SERDES_ERR_CONF_UNKNOWN,      /* Unknown configuration property */
        SERDES_ERR_CONF_INVALID,      /* Invalid configuration property value */
        SERDES_ERR_FRAMING_INVALID,   /* Invalid payload framing */
        SERDES_ERR_SCHEMA_LOAD,       /* Schema load failed */
        SERDES_ERR_PAYLOAD_INVALID,   /* Payload is invalid */
        SERDES_ERR_SCHEMA_MISMATCH,   /* Object does not match schema */
        SERDES_ERR_SCHEMA_REQUIRED,   /* Schema required to perform operation */
        SERDES_ERR_SERIALIZER,        /* Serializer failed */
        SERDES_ERR_BUFFER_SIZE        /* Inadequate buffer size */
} serdes_err_t;


/**
 * Returns the human readable form of a serdes_err_t
 * The returned pointer has infinite life time and must not be freed.
 */
SERDES_EXPORT
const char *serdes_err2str (serdes_err_t err);




/*******************************************************************************
 *
 * Configuration interface
 *
 ******************************************************************************/

/**
 * Frees a configuration prevoiusly returned by `serdes_conf_new()` or
 * `serdes_conf_copy()`.
 */
SERDES_EXPORT
void serdes_conf_destroy (serdes_conf_t *sconf);

/**
 * Returns a copy of `src`.
 */
SERDES_EXPORT
serdes_conf_t *serdes_conf_copy (const serdes_conf_t *src);


/**
 * Set single configuration property `name` to `value`.
 * Returns:
 *  `SERDES_ERR_OK` on success,
 *  `SERDES_ERR_CONF_INVALID` if `value` is invalid,
 *  `SERDES_ERR_CONF_UNKNOWN` if `name` is unknown.
 *
 * On error a human readable error description is written to `errstr`.
 */
SERDES_EXPORT
serdes_err_t serdes_conf_set (serdes_conf_t *sconf,
                              const char *name, const char *val,
                              char *errstr, int errstr_size);


/**
 * Set optional log callback to use for serdes originated log messages.
 */
SERDES_EXPORT
void serdes_conf_set_log_cb (serdes_conf_t *sconf,
                             void (*log_cb) (int level, const char *fac,
                                             const char *buf, void *log_opaque),
                             void *log_opaque);


/**
 * Creates a new configuration object with default settings.
 * The `...` var-args list is an optiona list of
 * `(const char *)name, (const char *)value` pairs that will be
 * automatically passed to `serdes_conf_set()` to set up the newly created
 * configuration object. If an error occurs setting a property NULL is returned
 * and a human readable error description is written to `errstr`.
 *
 * The var-args list must be terminated with a single NULL.
 *
 * If no initial configuration properties are to be set through var-args list
 * then call function as `serdes_conf_new(NULL, 0, NULL)`.
 */
SERDES_EXPORT
serdes_conf_t *serdes_conf_new (char *errstr, int errstr_size, ...);





/*******************************************************************************
 *
 * Schemas
 *
 ******************************************************************************/

/**
 * Remove schema from local cache and free memory.
 */
SERDES_EXPORT
void serdes_schema_destroy (serdes_schema_t *ss);


/**
 * Get and load schema from local cache or remote schema registry.
 * The schema may be looked up by its `name` or by its `id`.
 * `name` and `id` are mutually exclusive.
 * Null value for `name` is `NULL` and `-1` for `id`.
 *
 * The returned schema will be fully loaded and immediately usable.
 *
 * If the get or load fails NULL is returned and a human readable error
 * description is written to `errstr` of size `errstr_size`.
 */
SERDES_EXPORT
serdes_schema_t *serdes_schema_get (serdes_t *serdes, const char *name, int id,
                                    char *errstr, int errstr_size);


/**
 * Add schema definition to the local cache and stores the schema to remote
 * schema registry.
 *
 * The schema `name` is required, but the `id` is optional.
 * If `id` is set to -1 the schema will be stored on the remote schema
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
SERDES_EXPORT
serdes_schema_t *serdes_schema_add (serdes_t *sd, const char *name, int id,
                                    serdes_type_t type,
                                    const void *definition, int definition_len,
                                    char *errstr, int errstr_size);



/**
 * Returns the schema repository's id.
 */
SERDES_EXPORT
int serdes_schema_id (serdes_schema_t *schema);


/**
 * Returns the schema name.
 * The returned pointer is only valid until the schema is destroyed.
 * NULL is returned if the name of the schema is not known.
 */
SERDES_EXPORT
const char *serdes_schema_name (serdes_schema_t *schema);


/**
 * Returns the schema definition.
 * The returned pointer is only valid until the schema is destroyed.
 */
SERDES_EXPORT
const char *serdes_schema_definition (serdes_schema_t *schema);



/**
 * Purges any schemas that have not been used in `max_age` seconds.
 *
 * Returns the number of schemas removed.
 */
SERDES_EXPORT
int serdes_schemas_purge (serdes_t *serdes, int max_age);



/*******************************************************************************
 *
 * Main serdes handle
 *
 ******************************************************************************/

/**
 * Create a new serializer/deserializer handle of type `type` using
 * the optional `conf`.
 * If `conf` is non-NULL the serdes will assume ownership of the pointer
 * and the application shall consider the pointer freed.
 *
 * Use `serdes_destroy()` to free up resources associated with the serdes.
 */
SERDES_EXPORT
serdes_t *serdes_new (serdes_type_t type, serdes_conf_t *conf);


/**
 * Frees up any resources associated with the serdes, including schemas.
 * The serdes is no longer usable after this call.
 */
SERDES_EXPORT
void serdes_destroy (serdes_t *serdes);
