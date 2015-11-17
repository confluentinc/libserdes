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

#include "serdes.h"

#include <avro.h>


/*******************************************************************************
 *
 * Serializers and deserializers
 *
 ******************************************************************************/

/**
 * Serialize `avro` object into `*payloadp`.
 *
 * If `"serializer.framing"` is configured `schema` is required.
 * If `schema` is non-NULL the written object will be validated according
 * to the schema, if the validation fails SERDES_ERR_SCHEMA_MISMATCH
 * is returned.
 *
 * `payloadp` behaviour:
 *  - If `payloadp` is NULL no serialization is done but the required
 *    size of the payload buffer is returned in `*sizep`.
 *  - If `*payloadp` is NULL a buffer will be malloc(3):ed, it is the
 *    application's responsibility to free(3) this buffer when done with it.
 *  - If `*payloadp` is not NULL `*sizep` must be used to indicate the
 *    size of the `*payloadp` buffer.
 *
 * On success SERDES_ERR_OK is returned and `*payloadp` points to
 * the serialized payload, `*sizep` is set to the byte length of the payload.
 *
 * On error a SERDES_ERR_.. error is returned and a human readable
 * error description is writing to `errstr`.
 */
serdes_err_t serdes_schema_serialize_avro (serdes_schema_t *schema,
                                           avro_value_t *avro,
                                           void **payloadp, size_t *sizep,
                                           char *errstr, int errstr_size);

/**
 * Deserialize `payload` of `size` bytes using `schema`.
 *
 * On success SERDES_ERR_OK is returned and the decoded Avro object is
 * passed `avro`, the Avro object must be freed with `avro_value_decref()`
 * when the application is done with it.
 *
 * On error a SERDES_ERR_.. error is returned and a human readable
 * error description is written to `errstr`.
 */
serdes_err_t serdes_schema_deserialize_avro (serdes_schema_t *schema,
                                             avro_value_t *avro,
                                             const void *payload, size_t size,
                                             char *errstr, int errstr_size);


/**
 * Deserialize `payload` of `size`.
 *
 * Payload must be framed according the the `deserialize.framing` configuration
 *  property (see `serdes_conf_set()`) to allow lookup and load of the schema.
 *
 * The schema used is returned in `*schemap` (optional).
 *
 * Same error semantics as `serdes_schema_deserialize_avro()`
 */
serdes_err_t serdes_deserialize_avro (serdes_t *serdes, avro_value_t *avro,
                                      serdes_schema_t **schemap,
                                      const void *payload, size_t size,
                                      char *errstr, int errstr_size);





/**
 * Returns the avro_schema_t object for a schema.
 */
SERDES_EXPORT
const avro_schema_t serdes_schema_avro (serdes_schema_t *schema);

