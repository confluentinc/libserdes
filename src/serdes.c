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

#include <stdarg.h>

const char *serdes_err2str (serdes_err_t err) {
        switch (err)
        {
        case SERDES_ERR_OK:
                return "Success";
        case SERDES_ERR_CONF_UNKNOWN:
                return "Unknown configuration property";
        case SERDES_ERR_CONF_INVALID:
                return "Invalid configuration property value";
        case SERDES_ERR_FRAMING_INVALID:
                return "Invalid payload framing";
        case SERDES_ERR_PAYLOAD_INVALID:
                return "Payload is invalid";
        case SERDES_ERR_SCHEMA_LOAD:
                return "Schema load failed";
        case SERDES_ERR_SCHEMA_MISMATCH:
                return "Object does not match schema";
        case SERDES_ERR_SCHEMA_REQUIRED:
                return "Schema required to perform operation";
        case SERDES_ERR_SERIALIZER:
                return "Serializer failed";
        case SERDES_ERR_BUFFER_SIZE:
                return "Inadequate buffer size";
        default:
                return "(unknown error)";
        }
}


static void serdes_conf_destroy0 (serdes_conf_t *sconf) {
        url_list_clear(&sconf->schema_registry_urls);
}

void serdes_conf_destroy (serdes_conf_t *sconf) {
        serdes_conf_destroy0(sconf);
        free(sconf);
}


/**
 * Low-level serdes_conf_t copy
 */
static void serdes_conf_copy0 (serdes_conf_t *dst, const serdes_conf_t *src) {

        url_list_clear(&dst->schema_registry_urls);
        if (src->schema_registry_urls.str)
                url_list_parse(&dst->schema_registry_urls,
                               src->schema_registry_urls.str);
        dst->serializer_framing   = src->serializer_framing;
        dst->deserializer_framing = src->deserializer_framing;
        dst->debug   = src->debug;
        dst->schema_load_cb = src->schema_load_cb;
        dst->schema_unload_cb = src->schema_unload_cb;
        dst->log_cb  = src->log_cb;
        dst->opaque = src->opaque;
}

serdes_conf_t *serdes_conf_copy (const serdes_conf_t *src) {
        serdes_conf_t *dst;
        dst = serdes_conf_new(NULL, 0, NULL);
        serdes_conf_copy0(dst, src);
        return dst;
}


serdes_err_t serdes_conf_set (serdes_conf_t *sconf,
                              const char *name, const char *val,
                              char *errstr, int errstr_size) {

        if (!strcmp(name, "schema.registry.url")) {
                url_list_clear(&sconf->schema_registry_urls);
                if (url_list_parse(&sconf->schema_registry_urls, val) == 0) {
                        snprintf(errstr, errstr_size,
                                 "Invalid value for %s", name);
                        return SERDES_ERR_CONF_INVALID;
                }

        } else if (!strcmp(name, "serializer.framing") ||
                   !strcmp(name, "deserializer.framing")) {
                int framing;
                if (!strcmp(val, "none"))
                        framing = SERDES_FRAMING_NONE;
                else if (!strcmp(val, "cp1"))
                        framing = SERDES_FRAMING_CP1;
                else {
                        snprintf(errstr, errstr_size,
                                 "Invalid value for %s, allowed values: "
                                 "cp1, none", name);
                        return SERDES_ERR_CONF_INVALID;
                }

                if (!strcmp(name, "serializer.framing"))
                        sconf->serializer_framing = framing;
                else
                        sconf->deserializer_framing = framing;

        } else if (!strcmp(name, "debug")) {
                if (!strcmp(val, "all"))
                        sconf->debug = 1;
                else if (!strcmp(val, "") || !strcmp(val, "none"))
                        sconf->debug = 0;
                else {
                        snprintf(errstr, errstr_size,
                                 "Invalid value for %s, allowed values: "
                                 "all, none", name);
                        return SERDES_ERR_CONF_INVALID;
                }
        } else {
                snprintf(errstr, errstr_size,
                         "Unknown configuration property %s", name);
                return SERDES_ERR_CONF_UNKNOWN;
        }

        return SERDES_ERR_OK;
}


void serdes_conf_set_schema_load_cb (serdes_conf_t *sconf,
                                     void *(*load_cb) (serdes_schema_t *schema,
                                                       const char *definition,
                                                       size_t definition_len,
                                                       char *errstr,
                                                       size_t errstr_size,
                                                       void *opaque),
                                     void (*unload_cb) (serdes_schema_t *schema,
                                                        void *schema_obj,
                                                        void *opaque)) {
        sconf->schema_load_cb = load_cb;
        sconf->schema_unload_cb = unload_cb;
}


void serdes_conf_set_log_cb (serdes_conf_t *sconf,
                             void (*log_cb) (serdes_t *sd,
                                             int level, const char *fac,
                                             const char *buf, void *opaque)) {
        sconf->log_cb     = log_cb;
}

void serdes_conf_set_opaque (serdes_conf_t *sconf, void *opaque) {
        sconf->opaque = opaque;
}


/**
 * Initialize config object to default values
 */
static void serdes_conf_init (serdes_conf_t *sconf) {
        memset(sconf, 0, sizeof(*sconf));
        sconf->serializer_framing   = SERDES_FRAMING_CP1;
        sconf->deserializer_framing = SERDES_FRAMING_CP1;
}

serdes_conf_t *serdes_conf_new (char *errstr, int errstr_size, ...) {
        serdes_conf_t *sconf;
        va_list ap;
        const char *name, *val;

        sconf = malloc(sizeof(*sconf));
        serdes_conf_init(sconf);

        /* Chew through name,value pairs. */
        va_start(ap, errstr_size);
        while ((name = va_arg(ap, const char *))) {
                if (!(val = va_arg(ap, const char *))) {
                        snprintf(errstr, errstr_size,
                                 "Missing value for \"%s\"", name);
                        serdes_conf_destroy(sconf);
                        return NULL;
                }
                if (serdes_conf_set(sconf, name, val, errstr, errstr_size) !=
                    SERDES_ERR_OK) {
                        serdes_conf_destroy(sconf);
                        return NULL;
                }
        }
        va_end(ap);

        return sconf;
}




void serdes_log (serdes_t *sd, int level, const char *fac,
                 const char *fmt, ...) {
        va_list ap;
        char buf[512];

        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        if (sd->sd_conf.log_cb)
                sd->sd_conf.log_cb(sd, level, fac, buf, sd->sd_conf.opaque);
        else
                fprintf(stderr, "%% SERDES-%d-%s: %s\n", level, fac, buf);
}


void serdes_destroy (serdes_t *sd) {
        serdes_schema_t *ss;

        while ((ss = LIST_FIRST(&sd->sd_schemas)))
                serdes_schema_destroy(ss);

        serdes_conf_destroy0(&sd->sd_conf);

        mtx_destroy(&sd->sd_lock);
        free(sd);
}

serdes_t *serdes_new (serdes_conf_t *conf, char *errstr, size_t errstr_size) {
        serdes_t *sd;

        sd = calloc(1, sizeof(*sd));
        LIST_INIT(&sd->sd_schemas);
        mtx_init(&sd->sd_lock, mtx_plain);

        if (conf) {
                serdes_conf_copy0(&sd->sd_conf, conf);
                serdes_conf_destroy(conf);
        } else
                serdes_conf_init(&sd->sd_conf);

        if (!sd->sd_conf.schema_load_cb) {
#ifndef ENABLE_AVRO_C
                snprintf(errstr, errstr_size,
                         "No schema loader configured"
                         "(serdes_conf_set_schema_load_cb)");
                serdes_destroy(sd);
                return NULL;
#else
                /* Default schema loader to Avro-C */
                sd->sd_conf.schema_load_cb   = serdes_avro_schema_load_cb;
                sd->sd_conf.schema_unload_cb = serdes_avro_schema_unload_cb;
#endif
        }

        return sd;
}
