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

#include "serdes_int.h"
#include "serdes-avro.h"


serdes_err_t serdes_schema_deserialize_avro (serdes_schema_t *ss,
                                             avro_value_t *avro,
                                             const void *payload, size_t size,
                                             char *errstr, int errstr_size) {
        avro_reader_t reader;
        avro_value_iface_t *iface;
        serdes_err_t err = SERDES_ERR_OK;
        avro_schema_t avro_schema = ss->ss_schema_obj;

        reader = avro_reader_memory(payload, size);

        iface = avro_generic_class_from_schema(avro_schema);
        avro_generic_value_new(iface, avro);

        if (avro_value_read(reader, avro) != 0) {
                snprintf(errstr, errstr_size,
                         "Failed to read avro value: %s", avro_strerror());
                err = SERDES_ERR_PAYLOAD_INVALID;
        }

        avro_value_iface_decref(iface);
        avro_reader_free(reader);

        return err;
}





serdes_err_t serdes_deserialize_avro (serdes_t *sd, avro_value_t *avro,
                                      serdes_schema_t **schemap,
                                      const void *payload, size_t size,
                                      char *errstr, int errstr_size) {
        serdes_schema_t *ss;
        ssize_t r;

        /* "Schema-less" (we look up the schema for the application)
         * deserialization requires message framing so we can figure
         * out the schema id. */
        r = serdes_framing_read(sd, &payload, &size, &ss, errstr, errstr_size);
        if (r == -1)
                return SERDES_ERR_PAYLOAD_INVALID;
        else if (r == 0) {
                snprintf(errstr, errstr_size,
                         "\"deserializer.framing\" not configured");
                return SERDES_ERR_SCHEMA_REQUIRED;
        }

        if (schemap)
                *schemap = ss;

        /* Deserialize payload to Avro object */
        return serdes_schema_deserialize_avro(ss, avro, payload, size,
                                              errstr, errstr_size);
}
