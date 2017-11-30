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


void *serdes_avro_schema_load_cb (serdes_schema_t *ss,
                                  const char *definition,
                                  size_t definition_len,
                                  char *errstr, size_t errstr_size,
                                  void *opaque) {
        avro_schema_t avro_schema;

        if (avro_schema_from_json_length(definition, definition_len,
                                         &avro_schema)) {
                snprintf(errstr, errstr_size, "%s", avro_strerror());
                return NULL;
        }

        return avro_schema;
}

void serdes_avro_schema_unload_cb (serdes_schema_t *ss, void *schema_obj,
                                   void *opaque) {
        avro_schema_t avro_schema = schema_obj;

        avro_schema_decref(avro_schema);
}

const avro_schema_t serdes_schema_avro (serdes_schema_t *ss) {
        return ss->ss_schema_obj;
}
