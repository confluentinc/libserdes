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

#include <sys/queue.h>

#include <avro.h>

#include "../config.h"
#include "tinycthread.h"
#include "serdes.h"
#include "rest.h"


#ifndef LOG_DEBUG
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7
#endif

/* Conditional Debugging macro */
#define DBG(SD,FAC,FMT...) do {                                 \
                if ((SD)->sd_conf.debug)                        \
                        serdes_log(SD, LOG_DEBUG, FAC, FMT);    \
        } while (0)


typedef enum {
        SERDES_FRAMING_NONE,
        SERDES_FRAMING_CP1      /* Confluent Platform framing:
                                 * [8-bit magic][32-bit schema id]*/
} serdes_framing_t;

/**
 * Configuration object
 */
struct serdes_conf_s {
        url_list_t  schema_registry_urls;      /* CSV list of schema
                                                * registry URLs. */
        int         debug;                     /* Debugging 1=enabled */


        serdes_framing_t   serializer_framing;   /* Serializer framing */
        serdes_framing_t deserializer_framing;   /* Deserializer framing */

        /* Schema load/unload callbacks */
        void *(*schema_load_cb) (serdes_schema_t *ss,
                                 const char *definition, size_t definition_len,
                                 char *errstr, size_t errstr_size,
                                 void *opaque);
        void (*schema_unload_cb) (serdes_schema_t *ss, void *schema_obj,
                                  void *opaque);
        void *opaque;

        /* Log callback */
        void      (*log_cb) (serdes_t *serdes,
                             int level, const char *fac, const char *str,
                             void *log_opaque);
};



/**
 * Main serdes handle
 */
struct serdes_s {
        mtx_t          sd_lock;                  /* Protects sd_schemas */
        LIST_HEAD(, serdes_schema_s) sd_schemas; /* Schema cache */

        struct serdes_conf_s sd_conf;                  /* Configuration */
};


/**
 * Cached schema.
 */
struct serdes_schema_s {
        LIST_ENTRY(serdes_schema_s) ss_link; /* serdes_t.sd_schemas list */
        int           ss_id;                 /* Schema registry's id of schema*/
        char         *ss_name;               /* Name of schema */

        char         *ss_definition;         /* Schema definition */
        int           ss_definition_len;     /* Schema definition length */

        time_t        ss_t_last_used;        /* Timestamp of last use. */

        void         *ss_schema_obj;         /* Schema object, type depends
                                              * on configured load_cb */

        int           ss_linked;             /* On sd_schemas list */
        mtx_t         ss_lock;               /* Protects ss_t_last_used */
        serdes_t     *ss_sd;                 /* Back-pointer to serdes_t */
        void         *ss_opaque;             /* Application opaque */
};



void serdes_log (serdes_t *sd, int level, const char *fac,
                 const char *fmt, ...);





#if ENABLE_AVRO_C
/**
 *
 * schema-avro.c
 * serialize-avro.c
 * deserialize-avro.c
 *
 */

void *serdes_avro_schema_load_cb (serdes_schema_t *ss,
                                  const char *definition,
                                  size_t definition_len,
                                  char *errstr, size_t errstr_size,
                                  void *opaque);
void serdes_avro_schema_unload_cb (serdes_schema_t *ss, void *schema_obj,
                                   void *opaque);
#endif
