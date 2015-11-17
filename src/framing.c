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

#include <arpa/inet.h> /* ntohl/htonl */


size_t serdes_serializer_framing_size (serdes_t *sd) {
        switch (sd->sd_conf.serializer_framing)
        {
        case SERDES_FRAMING_CP1:
                return 1+4;  /* magic+schema_id */
        default:
                return 0;
        }
}



/**
 * Write CP1 framing to `payload` which must be of at least size 5.
 * Returns the number of bytes written.
 */
static size_t serdes_framing_cp1_write (serdes_schema_t *ss, char *payload,
                                        size_t size) {
        int32_t schema_id;

        if (size < 5)
                return -1;

        /* Magic byte */
        payload[0] = 0;

        /* Schema ID */
        schema_id = htonl(ss->ss_id);
        memcpy(payload+1, &schema_id, 4);

        return 5;
}


size_t serdes_framing_write (serdes_schema_t *ss, char *payload, size_t size) {

        switch (ss->ss_sd->sd_conf.serializer_framing)
        {
        case SERDES_FRAMING_CP1:
                return serdes_framing_cp1_write(ss, payload, size);
        default:
                return 0;
        }
}





size_t serdes_deserializer_framing_size (serdes_t *sd) {
        switch (sd->sd_conf.deserializer_framing)
        {
        case SERDES_FRAMING_CP1:
                return 1+4;  /* magic+schema_id */
        default:
                return 0;
        }
}



/**
 * Read CP1 framing and extract the schema id.
 *
 * payloadp and sizep is updated to skip framing.
 */
static ssize_t serdes_framing_cp1_read (const void **payloadp, size_t *sizep,
                                        int *schema_idp,
                                        char *errstr, int errstr_size) {
        const char *payload = *payloadp;
        size_t size = *sizep;

        if (size < 5) {
                snprintf(errstr, errstr_size,
                         "Payload is smaller (%zd) than framing (%d)",
                         size, 5);
                return -1;
        }

        /* Magic byte */
        if (payload[0] != 0) {
                snprintf(errstr, errstr_size,
                         "Invalid CP1 magic byte %d, expected %d",
                         (uint8_t)payload[0], 0);
                return -1;
        }

        /* Schema ID */
        memcpy(schema_idp, payload+1, 4);
        *schema_idp = ntohl(*schema_idp);

        /* Seek past framing */
        payload += 5;
        size -= 5;
        *payloadp = (const void *)payload;
        *sizep    = size;

        return 5;
}





ssize_t serdes_framing_read (serdes_t *sd, const void **payloadp, size_t *sizep,
                             serdes_schema_t **schemap,
                             char *errstr, int errstr_size) {
        serdes_schema_t *schema = NULL;
        int schema_id = -1;
        ssize_t r;

        switch (sd->sd_conf.deserializer_framing)
        {
        case SERDES_FRAMING_CP1:
                r = serdes_framing_cp1_read(payloadp, sizep, &schema_id,
                                            errstr, errstr_size);
                break;

        case SERDES_FRAMING_NONE:
                r = 0;
                break;

        default:
                snprintf(errstr, errstr_size, "Unsupported framing type %d",
                         sd->sd_conf.deserializer_framing);
                r = -1;
                break;
        }

        if (r == -1)
                return -1; /* Error */
        else if (r == 0)
                return 0;  /* No framing */

        if (!(schema = serdes_schema_get(sd, NULL, schema_id,
                                         errstr, errstr_size)))
                return -1;

        if (schemap)
                *schemap = schema;

        return r;
}
