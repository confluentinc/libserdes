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

#include <cstdio>
#include <exception>

#include "serdescpp.h"
#include "serdescpp_int.h"


namespace Serdes {


static void log_cb_trampoline (serdes_t *sd, int level,
                               const char *fac,
                               const char *buf, void *opaque) {
  HandleImpl *hnd = static_cast<HandleImpl*>(opaque);
  hnd->log_cb_->log_cb(hnd, level, fac, buf);
}



/**
 * Common function to create a serdes handle based on conf.
 */
static int create_serdes (HandleImpl *hnd, const Conf *conf,
                          std::string &errstr) {
  const ConfImpl *confimpl = conf ? dynamic_cast<const ConfImpl*>(conf) : NULL;
  serdes_conf_t *sconf;
  char c_errstr[256];

  if (confimpl) {
    sconf = serdes_conf_copy(confimpl->conf_);

    serdes_conf_set_opaque(sconf, (void *)hnd);

    if (confimpl->log_cb_) {
      serdes_conf_set_log_cb(sconf, log_cb_trampoline);
      hnd->log_cb_ = confimpl->log_cb_;
    }

  } else {
    sconf = serdes_conf_new(NULL, 0, NULL);
  }

  hnd->sd_ = serdes_new(sconf, c_errstr, sizeof(c_errstr));
  if (!hnd->sd_) {
    if (sconf)
      serdes_conf_destroy(sconf);
    errstr = c_errstr;
    return -1;
  }

  return 0;
}

Handle *Handle::create (const Conf *conf, std::string &errstr) {
  HandleImpl *hnd = new HandleImpl();

  if (create_serdes(hnd, conf, errstr) == -1) {
    delete hnd;
    return NULL;
  }

  return hnd;
}


Conf *Conf::create () {
  ConfImpl *conf = new ConfImpl();
  conf->conf_ = serdes_conf_new(NULL, 0, NULL);
  return conf;
}


static Schema *schema_get (Handle *handle, const char *name, int id,
                           std::string &errstr) {
  HandleImpl *hnd = dynamic_cast<HandleImpl*>(handle);
  char c_errstr[512];

  serdes_schema_t *c_schema = serdes_schema_get(hnd->sd_, name, id,
                                                c_errstr, sizeof(c_errstr));
  if (!c_schema) {
    errstr = c_errstr;
    return NULL;
  }

  SchemaImpl *schemaimpl =
      static_cast<SchemaImpl*>(serdes_schema_opaque(c_schema));
  if (!schemaimpl) {
    schemaimpl = new SchemaImpl(c_schema);
    serdes_schema_set_opaque(c_schema, schemaimpl);
  }

  return schemaimpl;
}


static Schema *schema_add (Handle *handle, const char *name, int id,
                           const char *type,
                           const void *definition,
                           int definition_len,
                           std::string &errstr) {
  HandleImpl *hnd = dynamic_cast<HandleImpl*>(handle);
  char c_errstr[512];

  serdes_schema_t *c_schema = serdes_schema_add(hnd->sd_, name, id, type,
                                                definition, definition_len,
                                                c_errstr, sizeof(c_errstr));
  if (!c_schema) {
    errstr = c_errstr;
    return NULL;
  }

  SchemaImpl *schema = new SchemaImpl();
  schema->schema_ = c_schema;

  return schema;
}


Schema *Schema::get (Handle *handle, int id, std::string &errstr) {
  return schema_get(handle, NULL, id, errstr);
}


Schema *Schema::get (Handle *handle, const std::string &name,
                         std::string &errstr) {
  return schema_get(handle, name.c_str(), -1, errstr);
}

Schema *Schema::add (Handle *handle, int id,
                     const std::string &definition, std::string &errstr) {
  return schema_add(handle, NULL, id, NULL,
                    NULL, 0, errstr);
}

Schema *Schema::add (Handle *handle,
                         const std::string &name,
                         const std::string &type,
                         const std::string &definition,
                         std::string &errstr) {
  return schema_add(handle, name.c_str(), -1, type.c_str(),
                    definition.c_str(), definition.length(), errstr);

}

Schema *Schema::add (Handle *handle, const std::string &name, int id,
                         const std::string &type,
                         const std::string &definition,
                         std::string &errstr) {
  return schema_add(handle, name.c_str(), id, type.c_str(),
                    definition.c_str(), definition.length(), errstr);
}


}
