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

#include <arpa/inet.h>


serdes_err_t serdes_schema_deserialize_avro (serdes_schema_t *ss,
                                             avro_value_t *avro,
                                             const void *payload, size_t size,
                                             char *errstr, int errstr_size) {
        avro_reader_t reader;
        avro_value_iface_t *iface;
        serdes_err_t err = SERDES_ERR_OK;

        reader = avro_reader_memory(payload, size);

        iface = avro_generic_class_from_schema(ss->ss_avro.schema);
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



/**
 * Read CP1 framing and extract the schema id.
 *
 * payloadp and sizep is updated to skip framing.
 */
static serdes_err_t read_cp1_framing (const void **payloadp, size_t *sizep,
                                      int *schema_idp,
                                      char *errstr, int errstr_size) {
        const char *payload = *payloadp;
        size_t size = *sizep;

        if (size < 5) {
                snprintf(errstr, errstr_size,
                         "Payload is smaller (%zd) than framing (%d)",
                         size, 5);
                return SERDES_ERR_FRAMING_INVALID;
        }

        /* Magic byte */
        if (payload[0] != 0) {
                snprintf(errstr, errstr_size,
                         "Invalid CP1 magic byte %d, expected %d",
                         (uint8_t)payload[0], 0);
                return SERDES_ERR_FRAMING_INVALID;
        }

        /* Schema ID */
        memcpy(schema_idp, payload+1, 4);
        *schema_idp = ntohl(*schema_idp);

        /* Seek past framing */
        payload += 5;
        size -= 5;
        *payloadp = (const void *)payload;
        *sizep    = size;

        return SERDES_ERR_OK;
}


serdes_err_t serdes_deserialize_avro (serdes_t *sd, avro_value_t *avro,
                                      serdes_schema_t **schemap,
                                      const void *payload, size_t size,
                                      char *errstr, int errstr_size) {
        serdes_schema_t *ss;
        int schema_id;
        serdes_err_t err;

        /* "Schema-less" (we look up the schema for the application)
         * deserialization requires message framing so we can figure
         * out the schema id. */
        switch (sd->sd_conf.deserializer_framing)
        {
        case SERDES_FRAMING_CP1:
                err = read_cp1_framing(&payload, &size, &schema_id,
                                       errstr, errstr_size);
                if (err)
                        return err;
                break;

        case SERDES_FRAMING_NONE:
        default:
                snprintf(errstr, errstr_size,
                         "\"deserializer.framing\" not configured");
                return SERDES_ERR_CONF_INVALID;
        }

        /* Load the schema */
        ss = serdes_schema_get(sd, NULL, schema_id, errstr, errstr_size);
        if (!ss)
                return SERDES_ERR_SCHEMA_LOAD;

        if (schemap)
                *schemap = ss;

        /* Deserialize payload to Avro object */
        return serdes_schema_deserialize_avro(ss, avro, payload, size,
                                              errstr, errstr_size);
}
