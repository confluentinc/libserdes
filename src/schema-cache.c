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

#define DEFINTION_MAX 10000
#define MAX_JSON_LENGTH 10000

#define MAX_SCHEMA_LENGTH 2048

char *addEscapes(const char *input)
{
    size_t inputLength  = strlen(input);
    size_t outputLength = inputLength;
    for (size_t i = 0; i < inputLength; i++)
    {
        if (input[i] == '\"')
        {
            outputLength++;
        }
    }

    char  *output = (char *)malloc((outputLength + 1) * sizeof(char));
    size_t j      = 0;

    for (size_t i = 0; i < inputLength; i++)
    {
        if (input[i] == '\"')
        {
            output[j++] = '\\';
        }
        output[j++] = input[i];
    }

    output[outputLength] = '\0';

    return output;
}

char *getKeyName(const char *json)
{
    char *result = NULL;

    const char *namePos = strstr(json, "\\\"name\\\":\\\"");
    if (namePos != NULL)
    {
        namePos += strlen("\\\"name\\\":\\\"");

        const char *nameEnd = strstr(namePos, "\\\",");
        if (nameEnd != NULL)
        {
            size_t nameLength = nameEnd - namePos;

            result = (char *)malloc((nameLength + 1) * sizeof(char));
            strncpy(result, namePos, nameLength);
            result[nameLength] = '\0';
        }
    }

    return result;
}

char *getFieldFromType(const char *json, const char *type)
{
    char *result = NULL;

    const char *fieldsPos = strstr(json, "\\\"fields\\\":[");
    if (fieldsPos != NULL)
    {
        fieldsPos += strlen("\\\"fields\\\":[");

        const char *fieldPos = fieldsPos;
        while ((fieldPos = strstr(fieldPos, "{\\\"name\\\":\\\"")) != NULL)
        {
            fieldPos += strlen("{\\\"name\\\":\\\"");

            const char *fieldNameEnd = strstr(fieldPos, "\\\"");
            if (fieldNameEnd != NULL)
            {
                size_t fieldNameLength = fieldNameEnd - fieldPos;

                const char *fieldTypePos = strstr(fieldPos, "\\\",\\\"type\\\":\\\"");
                if (fieldTypePos != NULL)
                {
                    fieldTypePos += strlen("\\\",\\\"type\\\":\\\"");

                    const char *fieldTypeEnd = strstr(fieldTypePos, "\\\"");
                    if (fieldTypeEnd != NULL)
                    {
                        size_t fieldTypeLength = fieldTypeEnd - fieldTypePos;

                        if (strncmp(type, fieldTypePos, fieldTypeLength) == 0)
                        {
                            result = (char *)malloc((fieldNameLength + 1) * sizeof(char));
                            strncpy(result, fieldPos, fieldNameLength);
                            result[fieldNameLength] = '\0';
                            return result;
                        }
                    }
                }
            }

            fieldPos++;
        }
    }

    return result;
}

char *modifyFieldInSchema(const char *schema, const char *fieldName, const char *fieldValue)
{

    char *modifiedSchema = malloc(MAX_SCHEMA_LENGTH);
    strcpy(modifiedSchema, schema);
    const char field[]    = "{\\\"name\\\":\\\"";
    char      *fieldStart = strstr(modifiedSchema, field);

    while (fieldStart != NULL)
    {
        fieldStart += strlen("\\\"name\\\":\\\"");
        char *fieldEnd = strstr(fieldStart, "\\\",");
        
        if (fieldEnd != NULL)
        {
            int   fieldNameLength   = fieldEnd - fieldStart;
            char *existingFieldName = malloc(fieldNameLength + 1);
            strncpy(existingFieldName, fieldStart, fieldNameLength);
            existingFieldName[fieldNameLength] = '\0';
            if (strcmp(existingFieldName, fieldName) == 0)
            {

                char *fieldTypeStart = strstr(fieldEnd, ",\\\"type\\\":\\\"");

                if (fieldTypeStart != NULL)
                {
                    fieldTypeStart += strlen(",\\\"type\\\":");
                    char *fieldTypeEnd = strchr(fieldTypeStart, '\\\"},');
                    if (fieldTypeEnd != NULL)
                    {
                        int fieldTypeLength = fieldTypeEnd - fieldTypeStart;

                        int modifiedSchemaLength = strlen(modifiedSchema);
                        int fieldReplacementLength =
                            modifiedSchemaLength - (fieldTypeStart - modifiedSchema) + strlen(fieldValue) + 1;

                        char *modifiedFieldType = malloc(fieldReplacementLength);
                        snprintf(modifiedFieldType, fieldReplacementLength, "%s%s", fieldValue, fieldTypeEnd);
                        // Replace the field type in the modified schema
                        memcpy(fieldTypeStart, modifiedFieldType, fieldReplacementLength);

                        free(modifiedFieldType);
                        free(existingFieldName);
                        return modifiedSchema;
                    }
                }
            }

            free(existingFieldName);
        }

        fieldStart = strstr(fieldEnd, "\\\"name\\\":\\\"");
    }

    return modifiedSchema;
}

int countOccurrences(const char *str, const char *word)
{
    int count      = 0;
    int wordLength = strlen(word);
    // character by character
    while (*str)
    {
        // If the current character is not a letter, move to the next one
        while (*str && !isalpha(*str))
        {
            str++;
        }
        // Check if the desired word starts at this position
        if (strncmp(str, word, wordLength) == 0)
        {
            count++;
        }
        // Move to the next character
        str++;
    }

    return count;
}
void extract_subjects_and_versions(const char *json_string, char **subjects, int *versions)
{
    const char *subject_key = "\"subject\":\"";
    const char *version_key = "\"version\":";

    const char *start_pos = json_string;
    const char *end_pos   = NULL;
    int         i         = 0;
    // While substring subject_key is found in the main string start_pos
    while ((start_pos = strstr(start_pos, subject_key)) != NULL)
    {
        start_pos += strlen(subject_key);
        end_pos = strchr(start_pos, '\"');
        if (end_pos == NULL)
        {
            break;
        }
        size_t subject_length = end_pos - start_pos;
        char  *subject        = malloc((subject_length + 1) * sizeof(char));
        strncpy(subject, start_pos, subject_length);
        subject[subject_length] = '\0';

        start_pos = strstr(start_pos, version_key);
        if (start_pos == NULL)
        {
            break;
        }
        start_pos += strlen(version_key);
        char *end_ptr;
        // convert to int value
        int version = strtol(start_pos, &end_ptr, 10);
        if (end_ptr == start_pos)
        {
            break;
        }
        // Fill the array
        subjects[i] = subject;
        versions[i] = version;
        i++;
    }
}
char *getValueFromJSON(const char *payload, const char *key)
{
    if (payload == NULL || key == NULL)
    {
        return NULL;
    }

    char *keyStart = strstr(payload, key);
    if (keyStart == NULL)
    {

        return NULL;
    }

    char *valueStart = strchr(keyStart, '{');
    if (valueStart == NULL)
    {
        return NULL;
    }

    // Skip whitespace characters
    while (*valueStart != '\0' && (*valueStart == ':' || *valueStart == ' ' || *valueStart == '\t' ||
                                   *valueStart == '\n' || *valueStart == '\r'))
    {
        valueStart++;
    }

    if (*valueStart == '\0')
    {
        return NULL;
    }

    char *valueEnd = strchr(valueStart, ']');
    if (valueEnd == NULL)
    {
        valueEnd = strchr(valueStart, '}');
        if (valueEnd == NULL)
        {
            return NULL;
        }
    }
    // Copy the value to a new string
    int   valueLength = valueEnd - valueStart + 2;
    char *valueStr    = malloc(valueLength + 1);
    strncpy(valueStr, valueStart, valueLength);
    valueStr[valueLength] = '\0';
    return valueStr;
}
void recursive_search(serdes_schema_t *ss, char *definition, const char *json_string, int len_json_string)
{
    char             json[100000] = "";
    const char      *Field;
    const char      *secondField;
    serdes_t        *sd = ss->ss_sd;
    char            *errstr;
    int              errstr_size;
    rest_response_t *rr;
    int              count        = 0;
    char            *valuePayload = getValueFromJSON(json_string, "schema");
    char            *valueRef     = getValueFromJSON(json_string, "references");
    if (valueRef == NULL)
    {

        if (valuePayload == NULL)
        {
            return;
        }
        strncpy(definition,json_string,len_json_string);

    }
    else
    {
        recursive_search(ss, definition, valueRef, strlen(valueRef));
        count = countOccurrences(valueRef, "name");
        char *subjects[count];
        int   versions[count];
        extract_subjects_and_versions(valueRef, subjects, versions);
        rest_response_t *rrsubject;

        for (int i = 0; i < count; i++)
        {
            rrsubject =
                rest_get(&sd->sd_conf.schema_registry_urls, "/subjects/%s/versions/%d", subjects[i], versions[i]);
            if (rest_response_failed(rrsubject))
            {
                rest_response_strerror(rrsubject, errstr, errstr_size);
                rest_response_destroy(rrsubject);
                return -1;
            }
            char *valueSchema = getValueFromJSON(rrsubject->payload, "schema");
            if (getKeyName(valueSchema) != NULL)
            {
                char *FieldName = getFieldFromType(valuePayload, getKeyName(valueSchema));
                Field           = modifyFieldInSchema(valuePayload, FieldName, valueSchema);
                valuePayload    = Field;
            }
            free(subjects[i]);
            free(valueSchema);
        }
        strcpy(definition, Field);
        char def[MAX_SCHEMA_LENGTH];
        strcat(def, "{\"schema\":\"");
        strcat(def, definition);
        strcat(def, "\"}");
        strcpy(definition, def);
    }

    free(valueRef);
    free(valuePayload);
}

/**
 * Update schema's timestamp of last use.
 */
static __inline void serdes_schema_mark_used(serdes_schema_t *ss)
{
    mtx_lock(&ss->ss_lock);
    ss->ss_t_last_used = time(NULL);
    mtx_unlock(&ss->ss_lock);
}

/**
 * Sets the schema's definition
 */
static void serdes_schema_set_definition(serdes_schema_t *ss, const void *definition, int len)
{
    if (ss->ss_definition)
    {
        free(ss->ss_definition);
        ss->ss_definition = NULL;
    }

    if (definition)
    {
        if (len == -1)
            len = strlen(definition);
        ss->ss_definition     = malloc(len + 1);
        ss->ss_definition_len = len;
        memcpy(ss->ss_definition, definition, len);
        ss->ss_definition[len] = '\0';
    }
}
/**
 * Destroy schema, sd_lock must be held.
 */
void serdes_schema_destroy0(serdes_schema_t *ss)
{

    if (ss->ss_schema_obj)
        ss->ss_sd->sd_conf.schema_unload_cb(ss, ss->ss_schema_obj, ss->ss_sd->sd_conf.opaque);

    serdes_schema_set_definition(ss, NULL, 0);

    if (ss->ss_name)
        free(ss->ss_name);

    if (ss->ss_linked)
        LIST_REMOVE(ss, ss_link);

    mtx_destroy(&ss->ss_lock);
    free(ss);
}

/**
 * Public API
 */
void serdes_schema_destroy(serdes_schema_t *ss)
{
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
static int serdes_schema_store(serdes_schema_t *ss, char *errstr, int errstr_size)
{
    serdes_t        *sd = ss->ss_sd;
    rest_response_t *rr;
    json_t          *json, *json_id;
    int              enc_len;
    char            *enc;
    json_error_t     err;

    if (sd->sd_conf.schema_registry_urls.cnt == 0)
    {
        snprintf(errstr, errstr_size,
                 "Unable to store schema %d at registry: "
                 "no 'schema.registry.url' configured",
                 ss->ss_id);
        return -1;
    }

    /* Encode JSON envelope */
    json = json_object();
    json_object_set_new(json, "schema", json_string(ss->ss_definition));
    enc     = json_dumps(json, JSON_COMPACT);
    enc_len = strlen(enc);

    /* POST schema definition to remote schema registry */
    rr = rest_post(&sd->sd_conf.schema_registry_urls, enc, enc_len, "/subjects/%s/versions", ss->ss_name);

    free(enc);
    json_decref(json);

    if (rest_response_failed(rr))
    {
        rest_response_strerror(rr, errstr, errstr_size);
        rest_response_destroy(rr);
        return -1;
    }

    /* Parse JSON response */
    if (!(json = json_loadb(rr->payload, rr->len, 0, &err)))
    {
        snprintf(errstr, errstr_size,
                 "Failed to read schema id: %s "
                 "at line %d, column %d",
                 err.text, err.line, err.column);
        rest_response_destroy(rr);
        return -1;
    }

    /* Get the returned schema id */
    if (!(json_id = json_object_get(json, "id")) || !json_is_integer(json_id))
    {
        snprintf(errstr, errstr_size, "No \"id\" int field in schema POST response");
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
 * Loads schema definition
 *
 * Returns -1 on failure.
 */
static int serdes_schema_load(serdes_schema_t *ss, const char *definition, size_t definition_len, char *errstr,
                              int errstr_size)
{
    serdes_t *sd      = ss->ss_sd;
    char     *wrapped = NULL;

    /* Left-trim schema definition */
    while (definition_len > 0 && isspace(*definition))
    {
        definition++;
        definition_len--;
    }

    /* Workaround: avro-c does not support string-based schemas, so we need to
     *             convert it to an object-based schema.
     *             https://issues.apache.org/jira/browse/AVRO-1691 */
    if (definition_len > 0 && *definition == '\"')
    {
        wrapped        = malloc(strlen("{ \"type\":   }") + definition_len + 1);
        definition_len = sprintf(wrapped, "{ \"type\": %s }", definition);
        definition     = wrapped;
    }

    DBG(ss->ss_sd, "SCHEMA_LOAD", "Received schema %s (%d) definition%s: %.*s", ss->ss_name, ss->ss_id,
        wrapped ? " (wrapped)" : "", (int)definition_len, definition);
    /* Parse schema */
    ss->ss_schema_obj =
        sd->sd_conf.schema_load_cb(ss, definition, definition_len, errstr, errstr_size, sd->sd_conf.opaque);
    if (!ss->ss_schema_obj)
    {

        DBG(ss->ss_sd, "SCHEMA_LOAD", "Schema load of %s failed: %s", ss->ss_name, errstr);
        if (wrapped)
            free(wrapped);

        return -1;
    }

    serdes_schema_set_definition(ss, definition, definition_len);

    if (wrapped)
        free(wrapped);

    return 0;
}

/**
 * Fetch schema definition from schema registry.
 *
 * Returns -1 on failure.
 */
static int serdes_schema_fetch(serdes_schema_t *ss, char *errstr, int errstr_size)
{
    serdes_t        *sd = ss->ss_sd;
    rest_response_t *rr;
    json_t          *json, *json_schema;
    json_error_t     err;

    if (sd->sd_conf.schema_registry_urls.cnt == 0)
    {
        snprintf(errstr, errstr_size,
                 "Unable to load schema %d from registry: "
                 "no 'schema.registry.url' configured",
                 ss->ss_id);
        return -1;
    }

    if (ss->ss_id != -1)
    {
        /* GET schema definition by id from remote schema registry */
        rr = rest_get(&sd->sd_conf.schema_registry_urls, "/schemas/ids/%d", ss->ss_id);
    }
    else
    {
        /* GET schema definition by name from remote schema registry */
        rr = rest_get(&sd->sd_conf.schema_registry_urls, "/subjects/%s/versions/latest", ss->ss_name);
    }

    if (rest_response_failed(rr))
    {
        rest_response_strerror(rr, errstr, errstr_size);
        rest_response_destroy(rr);
        return -1;
    }
    // Recursive search
    char definition[DEFINTION_MAX] = "";
    recursive_search(ss, definition, rr->payload, rr->len);

    if (!(json = json_loadb(definition, strlen(definition), 0, &err)))
    {
        snprintf(errstr, errstr_size,
                 "Failed to read schema envelope: %s "
                 "at line %d, column %d",
                 err.text, err.line, err.column);
        rest_response_destroy(rr);
        return -1;
    }
    /* Find schema definition in envelope */
    if (!(json_schema = json_object_get(json, "schema")) || !json_is_string(json_schema))
    {
        snprintf(errstr, errstr_size, "No \"schema\" string field in schema %d envelope", ss->ss_id);
        rest_response_destroy(rr);
        if (json_schema)
            json_decref(json_schema);
        json_decref(json);
        return -1;
    }
    if (ss->ss_id == -1)
    {
        /* Extract ID from response */
        json_t *json_id;

        if (!(json_id = json_object_get(json, "id")) || !json_is_integer(json_id))
        {
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
    }

    if (serdes_schema_load(ss, json_string_value(json_schema), strlen(json_string_value(json_schema)), errstr,
                           errstr_size) == -1)
    {
        rest_response_destroy(rr);
        json_decref(json);
        return -1;
    }

    DBG(ss->ss_sd, "SCHEMA_FETCH", "Succesfully fetched schema %s id %d: %s",
        ss->ss_name ? ss->ss_name : "(unknown-name)", ss->ss_id, json_string_value(json_schema));

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
static serdes_schema_t *serdes_schema_add0(serdes_t *sd, const char *name, int id, const void *definition,
                                           int definition_len, char *errstr, int errstr_size)
{

    serdes_schema_t *ss;

    if (id == -1 && !name)
    {
        snprintf(errstr, errstr_size, "Schema name or ID required");
        return NULL;
    }

    ss        = calloc(1, sizeof(*ss));
    ss->ss_id = id;
    ss->ss_sd = sd;

    if (name)
        ss->ss_name = strdup(name);

    if (definition)
    {
        if (!ss->ss_name)
        {
            snprintf(errstr, errstr_size, "Schema name required");
            free(ss);
            return NULL;
        }

        if (serdes_schema_load(ss, definition, definition_len, errstr, errstr_size) == -1)
        {
            serdes_schema_destroy0(ss);
            return NULL;
        }

        if (ss->ss_id == -1)
        {
            if (serdes_schema_store(ss, errstr, errstr_size) == -1)
            {
                serdes_schema_destroy0(ss);
                return NULL;
            }
        }
    }
    else
    {
        /* Fetch schema from registry, if any. */
        if (serdes_schema_fetch(ss, errstr, errstr_size) == -1)
        {
            serdes_schema_destroy0(ss);
            return NULL;
        }
    }

    mtx_init(&ss->ss_lock, mtx_plain);

    LIST_INSERT_HEAD(&sd->sd_schemas, ss, ss_link);
    ss->ss_linked = 1;

    return ss;
}

static serdes_schema_t *serdes_schema_find_by_id(serdes_t *sd, int id, int do_lock)
{
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

static serdes_schema_t *serdes_schema_find_by_definition(serdes_t *sd, const char *definition, int definition_len,
                                                         int do_lock)
{
    serdes_schema_t *ss;

    if (do_lock)
        mtx_lock(&sd->sd_lock);
    LIST_FOREACH(ss, &sd->sd_schemas, ss_link)
    {
        if (ss->ss_definition_len == definition_len && !memcmp(ss->ss_definition, definition, definition_len))
            break;
    }
    if (do_lock)
        mtx_unlock(&sd->sd_lock);

    return ss;
}

serdes_schema_t *serdes_schema_add(serdes_t *sd, const char *name, int id, const void *definition, int definition_len,
                                   char *errstr, int errstr_size)
{
    serdes_schema_t *ss;

    if (definition && definition_len == -1)
        definition_len = strlen(definition);

    mtx_lock(&sd->sd_lock);
    if (!(ss = serdes_schema_find_by_definition(sd, definition, definition_len, 0 /*no-lock*/)))
        ss = serdes_schema_add0(sd, name, id, definition, definition_len, errstr, errstr_size);
    mtx_unlock(&sd->sd_lock);

    if (ss)
        serdes_schema_mark_used(ss);
    return ss;
}

serdes_schema_t *serdes_schema_get(serdes_t *sd, const char *name, int id, char *errstr, int errstr_size)
{
    serdes_schema_t *ss;
    mtx_lock(&sd->sd_lock);
    if ((ss = serdes_schema_find_by_id(sd, id, 0 /*no-lock*/)))
    {
        mtx_unlock(&sd->sd_lock);
        serdes_schema_mark_used(ss);

        return ss;
    }

    ss = serdes_schema_add0(sd, name, id, NULL, 0, errstr, errstr_size);
    mtx_unlock(&sd->sd_lock);

    return ss; /* May be NULL */
}

int serdes_schema_id(serdes_schema_t *schema)
{
    return schema->ss_id;
}

const char *serdes_schema_name(serdes_schema_t *schema)
{
    return schema->ss_name;
}

const char *serdes_schema_definition(serdes_schema_t *schema)
{
    return schema->ss_definition;
}

void *serdes_schema_object(serdes_schema_t *schema)
{
    return schema->ss_schema_obj;
}

serdes_t *serdes_schema_handle(serdes_schema_t *schema)
{
    return schema->ss_sd;
}

int serdes_schemas_purge(serdes_t *serdes, int max_age)
{
    serdes_schema_t *next, *ss;
    time_t           expiry = time(NULL) - max_age;
    int              cnt    = 0;

    mtx_lock(&serdes->sd_lock);
    next = LIST_FIRST(&serdes->sd_schemas);
    while (next)
    {
        ss   = next;
        next = LIST_NEXT(next, ss_link);

        if (ss->ss_t_last_used < expiry)
        {
            serdes_schema_destroy0(ss);
            cnt++;
        }
    }
    mtx_unlock(&serdes->sd_lock);

    return cnt;
}

void serdes_schema_set_opaque(serdes_schema_t *schema, void *opaque)
{
    schema->ss_opaque = opaque;
}

void *serdes_schema_opaque(serdes_schema_t *schema)
{
    return schema->ss_opaque;
}
