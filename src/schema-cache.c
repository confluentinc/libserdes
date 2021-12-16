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

#include <ctype.h>

#include <jansson.h>

#include "serdes_int.h"
#include "rest.h"



/**
 * Update schema's timestamp of last use.
 */
static __inline void serdes_schema_mark_used (serdes_schema_t *ss) {
        mtx_lock(&ss->ss_lock);
        ss->ss_t_last_used = time(NULL);
        mtx_unlock(&ss->ss_lock);
}



/**
 * Sets the schema's definition
 */
static void serdes_schema_set_definition (serdes_schema_t *ss,
                                          const void *definition, int len) {
        if (ss->ss_definition) {
                free(ss->ss_definition);
                ss->ss_definition = NULL;
        }

        if (definition) {
                if (len == -1)
                        len = strlen(definition);
                ss->ss_definition = malloc(len+1);
                ss->ss_definition_len = len;
                memcpy(ss->ss_definition, definition, len);
                ss->ss_definition[len] = '\0';
        }
}


/**
 * Destroy schema, sd_lock must be held.
 */
void serdes_schema_destroy0 (serdes_schema_t *ss) {

        serdes_schema_set_definition(ss, NULL, 0);

        if (ss->ss_name)
                free(ss->ss_name);

        if (ss->ss_type)
                free(ss->ss_type);

        if (ss->ss_linked)
                LIST_REMOVE(ss, ss_link);

        mtx_destroy(&ss->ss_lock);
        free(ss);
}


/**
 * Public API
 */
void serdes_schema_destroy (serdes_schema_t *ss) {
        serdes_t *sd = ss->ss_sd;
        mtx_lock(&sd->sd_lock);
        serdes_schema_destroy0(ss);
        mtx_unlock(&sd->sd_lock);
}




/**
 * Store schema definition at schema registry.
 *
 * Returns -1 on failure.
 */
static int serdes_schema_store (serdes_schema_t *ss,
                                const void *definition, int definition_len,
                                char *errstr, int errstr_size) {
        serdes_t *sd = ss->ss_sd;
        rest_response_t *rr;
        json_t *json, *json_id;
        int enc_len;
        char *enc;
        json_error_t err;

        if (sd->sd_conf.schema_registry_urls.cnt == 0) {
                snprintf(errstr, errstr_size,
                         "Unable to store schema %d at registry: "
                         "no 'schema.registry.url' configured",
                         ss->ss_id);
                return -1;
        }

        /* Encode JSON envelope */
        json = json_object();
        json_object_set_new(json, "schema",
                            json_stringn(definition, definition_len));
        if (ss->ss_type)
                json_object_set_new(json, "schemaType",
                                    json_string(ss->ss_type));
        enc = json_dumps(json, JSON_COMPACT);
        enc_len = strlen(enc);

        /* POST schema definition to remote schema registry */
        rr = rest_post(&sd->sd_conf.schema_registry_urls, enc, enc_len,
                       "/subjects/%s/versions", ss->ss_name);

        free(enc);
        json_decref(json);

        if (rest_response_failed(rr)) {
                rest_response_strerror(rr, errstr, errstr_size);
                rest_response_destroy(rr);
                return -1;
        }

        /* Parse JSON response */
        if (!(json = json_loadb(rr->payload, rr->len, 0, &err))) {
                snprintf(errstr, errstr_size,
                         "Failed to read schema id: %s "
                         "at line %d, column %d",
                         err.text, err.line, err.column);
                rest_response_destroy(rr);
                return -1;
        }

        /* Get the returned schema id */
        if (!(json_id = json_object_get(json, "id")) ||
            !json_is_integer(json_id)) {
                snprintf(errstr, errstr_size,
                         "No \"id\" int field in schema POST response");
                rest_response_destroy(rr);
                if (json_id)
                        json_decref(json_id);
                json_decref(json);
                return -1;
        }

        ss->ss_id = json_integer_value(json_id);

        json_decref(json);
        rest_response_destroy(rr);

        return 0;
}


/**
 * @brief Convert string (in-place) to upper-case.
 */
static void str_mk_upper (char *s) {
        for ( ; *s ; s++)
                *s = (char)toupper((int)*s);
}


/**
 * Fetch schema definition from schema registry.
 *
 * Returns -1 on failure.
 */
static int serdes_schema_fetch (serdes_schema_t *ss,
                                char *errstr, int errstr_size) {
        serdes_t *sd = ss->ss_sd;
        rest_response_t *rr;
        json_t *json, *json_schema, *json_type;
        json_error_t err;

        if (sd->sd_conf.schema_registry_urls.cnt == 0) {
                snprintf(errstr, errstr_size,
                         "Unable to load schema %d from registry: "
                         "no 'schema.registry.url' configured",
                         ss->ss_id);
                return -1;
        }

        if (ss->ss_id != -1) {
                /* GET schema definition by id from remote schema registry */
                rr = rest_get(&sd->sd_conf.schema_registry_urls,
                              "/schemas/ids/%d", ss->ss_id);
        } else {
                /* GET schema definition by name from remote schema registry */
                rr = rest_get(&sd->sd_conf.schema_registry_urls,
                              "/subjects/%s/versions/latest", ss->ss_name);
        }

        if (rest_response_failed(rr)) {
                rest_response_strerror(rr, errstr, errstr_size);
                rest_response_destroy(rr);
                return -1;
        }

        /* Parse JSON envelope */
        if (!(json = json_loadb(rr->payload, rr->len, 0, &err))) {
                snprintf(errstr, errstr_size,
                         "Failed to read schema envelope: %s "
                         "at line %d, column %d",
                         err.text, err.line, err.column);
                rest_response_destroy(rr);
                return -1;
        }

        /* Get schema type (if supported) */
        if (!(json_type = json_object_get(json, "schemaType"))) {
                /* Old schema-registry only supported Avro */
                ss->ss_type = strdup("AVRO");
        } else if (!json_is_string(json_type)) {
                snprintf(errstr, errstr_size,
                         "Schema \"type\" field should be string, not "
                         "(Jansson type) %d",
                         json_typeof(json_type));
                rest_response_destroy(rr);
                json_decref(json_type);
                json_decref(json);
                return -1;
        } else {
                ss->ss_type = strdup(json_string_value(json_type));
                /* Convert to upper-case */
                str_mk_upper(ss->ss_type);

                json_decref(json_type);
        }

        if (ss->ss_id == -1) {
                /* Extract ID from response */
                json_t *json_id;

                if (!(json_id = json_object_get(json, "id")) ||
                    !json_is_integer(json_id)) {
                        snprintf(errstr, errstr_size,
                                 "No \"id\" int field in "
                                 "subject \"%s\" envelope",
                                 ss->ss_name);
                        rest_response_destroy(rr);
                        if (json_id)
                                json_decref(json_id);
                        json_decref(json);
                        return -1;
                }

                ss->ss_id = json_integer_value(json_id);
                json_decref(json_id);
        }

        /* Find schema definition in envelope */
        if (!(json_schema = json_object_get(json, "schema")) ||
            !json_is_string(json_schema)) {
                snprintf(errstr, errstr_size,
                         "No \"schema\" string field in schema %d envelope",
                         ss->ss_id);
                rest_response_destroy(rr);
                if (json_schema)
                        json_decref(json_schema);
                json_decref(json);
                return -1;
        }

        serdes_schema_set_definition(ss,
                                     json_string_value(json_schema),
                                     json_string_length(json_schema));
        json_decref(json_schema);

        DBG(ss->ss_sd, "SCHEMA_FETCH",
            "Succesfully fetched schema %s id %d type %s: %s",
            ss->ss_name ? ss->ss_name : "(unknown-subject)",
            ss->ss_id, ss->ss_type, ss->ss_definition);

        json_decref(json);
        rest_response_destroy(rr);

        return 0;
}


/**
 * Adds and loads a schema.
 *
 * If a schema object is returned it is guaranteed to be fully loaded
 * and usable, if the load fails NULL is returned and the error is set
 * in 'errstr'.
 *
 * Locks: sd->sd_lock MUST be held.
 */
static serdes_schema_t *serdes_schema_add0 (serdes_t *sd,
                                            const char *name, int id,
                                            const char *type,
                                            const void *definition,
                                            int definition_len,
                                            char *errstr, int errstr_size) {

        serdes_schema_t *ss;

        if (id == -1 && !name) {
                snprintf(errstr, errstr_size,
                         "Schema name or ID required");
                return NULL;
        }

        ss = calloc(1, sizeof(*ss));
        ss->ss_id = id;
        ss->ss_sd = sd;

        if (name)
                ss->ss_name = strdup(name);

        if (type) {
                ss->ss_type = strdup(type);
                str_mk_upper(ss->ss_type);
        }

        if (definition) {
                if (!ss->ss_name) {
                        snprintf(errstr, errstr_size, "Schema name required");
                        free(ss);
                        return NULL;
                }

                if (ss->ss_id == -1) {
                        if (serdes_schema_store(ss,
                                                definition,
                                                definition_len,
                                                errstr,
                                                errstr_size) == -1) {
                                serdes_schema_destroy0(ss);
                                return NULL;
                        }
                }

                serdes_schema_set_definition(ss, definition, definition_len);

        } else {
                /* Fetch schema from registry, if any. */
                if (serdes_schema_fetch(ss, errstr, errstr_size) == -1) {
                        serdes_schema_destroy0(ss);
                        return NULL;
                }
        }

        assert(ss->ss_type != NULL);

        mtx_init(&ss->ss_lock, mtx_plain);

        LIST_INSERT_HEAD(&sd->sd_schemas, ss, ss_link);
        ss->ss_linked = 1;

        return ss;
}


static serdes_schema_t *serdes_schema_find_by_id (serdes_t *sd, int id,
                                                  int do_lock) {
        serdes_schema_t *ss;

        if (do_lock)
                mtx_lock(&sd->sd_lock);
        LIST_FOREACH(ss, &sd->sd_schemas, ss_link)
                if (ss->ss_id == id)
                        break;
        if (do_lock)
                mtx_unlock(&sd->sd_lock);

        return ss;
}

static serdes_schema_t *
serdes_schema_find_by_definition (serdes_t *sd,
                                  const char *definition, int definition_len,
                                  const char *type,
                                  int do_lock) {
        serdes_schema_t *ss;

        if (do_lock)
                mtx_lock(&sd->sd_lock);
        LIST_FOREACH(ss, &sd->sd_schemas, ss_link) {
                if (ss->ss_definition_len == definition_len &&
                    !strcmp(ss->ss_type, type) &&
                    !memcmp(ss->ss_definition, definition, definition_len))
                        break;
        }
        if (do_lock)
                mtx_unlock(&sd->sd_lock);

        return ss;
}

serdes_schema_t *serdes_schema_add (serdes_t *sd, const char *name, int id,
                                    const char *type,
                                    const void *definition, int definition_len,
                                    char *errstr, int errstr_size) {
        serdes_schema_t *ss;

        if (definition && definition_len == -1)
                definition_len = strlen(definition);

        mtx_lock(&sd->sd_lock);
        if (!(ss = serdes_schema_find_by_definition(sd, definition,
                                                    definition_len,
                                                    type,
                                                    0/*no-lock*/)))
                ss = serdes_schema_add0(sd, name, id, type,
                                        definition, definition_len,
                                        errstr, errstr_size);
        mtx_unlock(&sd->sd_lock);

        if (ss)
                serdes_schema_mark_used(ss);
        return ss;
}




serdes_schema_t *serdes_schema_get (serdes_t *sd, const char *name, int id,
                                    char *errstr, int errstr_size) {
        serdes_schema_t *ss;

        mtx_lock(&sd->sd_lock);
        if ((ss = serdes_schema_find_by_id(sd, id, 0/*no-lock*/))) {
                mtx_unlock(&sd->sd_lock);
                serdes_schema_mark_used(ss);
                return ss;
        }

        ss = serdes_schema_add0(sd, name, id, NULL, NULL, 0,
                                errstr, errstr_size);
        mtx_unlock(&sd->sd_lock);

        return ss; /* May be NULL */
}


int serdes_schema_id (serdes_schema_t *schema) {
        return schema->ss_id;
}

const char *serdes_schema_name (serdes_schema_t *schema) {
        return schema->ss_name;
}

const char *serdes_schema_type (serdes_schema_t *schema) {
        return schema->ss_type;
}


const char *serdes_schema_definition (serdes_schema_t *schema) {
        return schema->ss_definition;
}

void serdes_schema_set_object (serdes_schema_t *schema, void *object) {
        schema->ss_schema_obj = object;
}

void *serdes_schema_object (serdes_schema_t *schema) {
        return schema->ss_schema_obj;
}

serdes_t *serdes_schema_handle (serdes_schema_t *schema) {
        return schema->ss_sd;
}



int serdes_schemas_purge (serdes_t *serdes, int max_age) {
        serdes_schema_t *next, *ss;
        time_t expiry = time(NULL) - max_age;
        int cnt = 0;

        mtx_lock(&serdes->sd_lock);
        next = LIST_FIRST(&serdes->sd_schemas);
        while (next) {
                ss = next;
                next = LIST_NEXT(next, ss_link);

                if (ss->ss_t_last_used < expiry) {
                        serdes_schema_destroy0(ss);
                        cnt++;
                }
        }
        mtx_unlock(&serdes->sd_lock);

        return cnt;
}


void serdes_schema_set_opaque (serdes_schema_t *schema, void *opaque) {
        schema->ss_opaque = opaque;
}

void *serdes_schema_opaque (serdes_schema_t *schema) {
        return schema->ss_opaque;
}
