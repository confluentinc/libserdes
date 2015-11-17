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

/**
 * Example utility to show case libserdes C++ interface
 */

#include <ostream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <signal.h>

#include <avro/Decoder.hh>

/* Typical include path is <libserdes/serdescpp.h> */
#include "../src-cpp/serdescpp.h"

static int run = 1;
static int verbosity = 2;

#define FATAL(reason...) do {                           \
    std::cerr << "FATAL: " << reason << std::endl;      \
    exit(1);                                            \
  } while (0)



class ExampleLogCb : public Serdes::LogCb {
 public:
  void log_cb (Serdes::Handle *serdes, int level, const std::string &fac,
               const std::string &buf) {
    std::cout << "% SERDES-" << level << "-" << fac << ": " << buf << std::endl;
  }
};


static void usage (const std::string &me) {
  std::cerr << "Usage: " << me << " <options>\n"
      "\n"
      "Options:\n"
      " -r <schreg-urls>  Schema registry URL\n"
      " -s <schema-name>  Schema/subject name\n"
      " -S <schema-def>   Schema definition (JSON)\n"
      " -j <json blob>    JSON blob to encode or decode\n"
      " -X <n>=<v>        Set Serdes configuration\n"
      " -v                Increase verbosity\n"
      " -q                Decrease verbosity\n"
      "\n"
      "Examples:\n"
      "  Retrieve schema definition by name:\n"
      "   " << me << " -s the_schema_name\n"
      "  Retrieve schema definition by id:\n"
      "   " << me << " -s 1234\n"
      "\n"
      "  Define new schema:\n"
      "   " << me << " -s the_schema_name -S \"$(cat my_def.json)\"\n"
      "\n" << std::endl;
  exit(1);
}


/**
 * Read JSON from stdin, using the provided schema.
 */
static void decode_json (Serdes::Schema *schema, const std::string &json_str) {

  avro::DecoderPtr decoder;

  decoder = avro::jsonDecoder(*static_cast<avro::ValidSchema*>(schema->object()));

  std::auto_ptr<avro::InputStream> istream =
      avro::memoryInputStream((const uint8_t *)json_str.c_str(), json_str.size());

  decoder->init(*istream);
  try {
    std::string s = decoder->decodeString();
    std::cout << "Read: " << s << std::endl;
  } catch (const avro::Exception &e) {
    FATAL("Decode failed: " << e.what());
  }
}


static void sig_term (int sig) {
  run = 0;
}


int main (int argc, char **argv) {
  serdes_err_t err;
  int opt;
  int schema_id = -1;
  std::string schema_name, schema_def, json_blob;
  std::string errstr;
  Serdes::Schema *schema;


  /* Controlled termination */
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_term;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);


  /* Create serdes configuration object.
   * Configuration passed through -X prop=val will be set on this object,
   * which is later passed to the serdes handle creator. */
  Serdes::Conf *sconf = Serdes::Conf::create();

  /* Default URL */
  if (sconf->set("schema.registry.url", "http://localhost:8081", errstr))
    FATAL("Conf failed: " << errstr);

  /* Logger */
  ExampleLogCb LogCb;
  sconf->set(&LogCb);

  while ((opt = getopt(argc, argv, "r:s:S:j:X:vq")) != -1) {
    switch (opt)
    {
      case 'r':
        if (sconf->set("schema.registry.url", optarg, errstr) != SERDES_ERR_OK)
          FATAL("Failed to set registry.url: " << errstr);
        break;

      case 's':
        schema_name = optarg;
        break;

      case 'S':
        schema_def = optarg;
        break;

      case 'j':
        json_blob = optarg;
        break;

      case 'X':
        {
          char *t = strchr(optarg, '=');
          if (!t)
            FATAL("Expected -X property=value");
          *t = '\0';

          std::string name = optarg;
          std::string val  = t+1;

          err = sconf->set(name, val, errstr);
          if (err == SERDES_ERR_OK)
            break;

          FATAL(errstr);
        }
        break;

      case 'v':
        verbosity++;
        break;
      case 'q':
        verbosity--;
        break;

      default:
        std::cerr << "%% Unknown option -" << (char)opt << std::endl;
        usage(argv[0]);
    }
  }

  /*
   * Create serdes handle
   */
  Serdes::Handle *serdes = Serdes::Handle::create(sconf, errstr);
  if (!serdes)
    FATAL("Failed to create serdes handle: " << errstr);

  delete sconf;

  /* If schema name is an integer treat it as schema id. */
  if (!schema_name.empty() &&
      schema_name.find_first_not_of("0123456789") == std::string::npos) {
    schema_id = atoi(schema_name.c_str());
    schema_name.clear();
  }


  if (schema_def.empty()) {
    /* Query schema registry */

    std::cout << "Query schema: by name \"" << schema_name << "\" or id "
              << schema_id << std::endl;

    if (!schema_name.empty())
      schema = Serdes::Schema::get(serdes, schema_name, errstr);
    else if (schema_id != -1)
      schema = Serdes::Schema::get(serdes, schema_id, errstr);
    else
      FATAL("Expected schema -s <id> or -s <name>");

    if (!schema)
      FATAL("Failed to get schema: " << errstr);

    std::cout << "Schema \"" << schema->name() << "\" id " << schema->id() <<
        ": " << schema->definition() << std::endl;


    if (!json_blob.empty())
      decode_json(schema, json_blob);


  } else {
    /* Register new schema */

    std::cout << "Register new schema: " << schema_name << ": "
              << schema_def << std::endl;

    schema = Serdes::Schema::add(serdes, schema_name,
                                 schema_def, errstr);

    if (!schema)
      FATAL("Failed to register schema " << schema_name << ": " << errstr);

    std::cout << "Registered schema " << schema->name() << " with id "
              << schema->id() << std::endl;

  }

  if (schema)
    delete schema;

  delete serdes;

  return 0;

}
