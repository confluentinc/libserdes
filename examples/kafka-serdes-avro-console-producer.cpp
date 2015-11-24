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
 * Example Kafka producer to show case integration with libserdes C++ API
 */



#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <getopt.h>

/* Typical include path is <libserdes/serdescpp.h> */
#include "../src-cpp/serdescpp-avro.h"

#include <librdkafka/rdkafkacpp.h>


#include <avro/Encoder.hh>
#include <avro/Decoder.hh>
#include <avro/Generic.hh>
#include <avro/Specific.hh>
#include <avro/Exception.hh>


static bool run = true;
static int verbosity = 2;

#define FATAL(reason...) do {                           \
    std::cerr << "% FATAL: " << reason << std::endl;      \
    exit(1);                                            \
  } while (0)







class MyDeliveryReportCb : public RdKafka::DeliveryReportCb {
 public:
  void dr_cb (RdKafka::Message &msg) {
    switch (msg.err())
    {
      case RdKafka::ERR_NO_ERROR:
        if (verbosity > 2)
          std::cerr << "% Message produced (offset " << msg.offset() << ")"
                    << std::endl;
        break;

      default:
        std::cerr << "% Message delivery failed: " << msg.errstr() << std::endl;
    }
  }
};


/**
 * Convert JSON to Avro Datum.
 *
 * Returns 0 on success or -1 on failure.
 */
static int json2avro (Serdes::Schema *schema, const std::string &json,
                      avro::GenericDatum **datump) {

  avro::ValidSchema *avro_schema = schema->object();

  /* Input stream from json string */
  std::istringstream iss(json);
  std::auto_ptr<avro::InputStream> json_is = avro::istreamInputStream(iss);

  /* JSON decoder */
  avro::DecoderPtr json_decoder = avro::jsonDecoder(*avro_schema);

  avro::GenericDatum *datum = new avro::GenericDatum(*avro_schema);

  try {
    /* Decode JSON to Avro datum */
    json_decoder->init(*json_is);
    avro::decode(*json_decoder, *datum);

  } catch (const avro::Exception &e) {
    std::cerr << "% JSON to Avro transformation failed: "
              << e.what() << std::endl;
    return -1;
  }

  *datump = datum;

  return 0;
}


static __attribute__((noreturn))
void usage (const std::string me) {

  std::cerr <<
      "Usage: " << me << " [options]\n"
      "Produces Avro encoded messages to Kafka from JSON objects "
      "read from stdin (one per line)\n"
      "\n"
      "Options:\n"
      " -b <brokers..>    Kafka broker(s)\n"
      " -t <topic>        Kafka topic to produce to\n"
      " -p <partition>    Kafka partition (defaults to partitioner)\n"
      " -r <schreg-urls>  Schema registry URL\n"
      " -s <schema-name>  Schema/subject name\n"
      " -S <schema-def>   Schema definition (JSON)\n"
      " -X kafka.topic.<n>=<v> Set RdKafka topic configuration\n"
      " -X kafka.<n>=<v>  Set RdKafka global configuration\n"
      " -X <n>=<v>        Set Serdes configuration\n"
      " -v                Increase verbosity\n"
      " -q                Decrease verbosity\n"
      "\n"
      "\n"
      "Examples:\n"
      "  # Register schema and produce messages:\n"
      "  " << me << " -b mybroker -t mytopic -s my_schema -S \"$(cat schema.json)\"\n"
      "\n"
      "  # Use existing schema:\n"
      "  " << me << " -b mybroker -t mytopic -s my_schema\n"
      "\n";

  exit(1);
}

static void sig_term (int sig) {
  run = false;
}


int main (int argc, char **argv) {
  std::string errstr;
  std::string schema_name;
  std::string schema_def;
  std::string topic;
  int partition = -1;

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

  /* Default framing CP1 */
  if (sconf->set("serializer.framing", "cp1", errstr))
    FATAL("Conf failed: " << errstr);

  /* Create rdkafka configuration object.
   * Configured passed through -X kafka.prop=val will be set on this object. */
  RdKafka::Conf *kconf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

  /* Create rdkafka default topic configuration object.
   * Configuration passed through -X kafka.topic.prop=val will be set .. */
  RdKafka::Conf *tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);


  /* Command line argument parsing */
  int opt;
  while ((opt = getopt(argc, argv, "b:t:p:g:r:s:S:X:vq")) != -1) {
    switch (opt)
    {
      case 'b':
        if (kconf->set("bootstrap.servers", optarg, errstr) !=
            RdKafka::Conf::CONF_OK)
          FATAL(errstr);
        break;

      case 't':
        topic = optarg;
        break;

      case 'p':
        partition = std::atoi(optarg);
        break;

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

      case 'X':
        {
          char *t = strchr(optarg, '=');
          if (!t)
            FATAL("Expected -X property=value");
          *t = '\0';

          std::string name = optarg;
          std::string val  = t+1;

          if (!strncmp(name.c_str(), "kafka.topic.", 12)) {
            RdKafka::Conf::ConfResult kres;

            kres = tconf->set(name.substr(12), val, errstr);
            if (kres == RdKafka::Conf::CONF_INVALID)
              FATAL(errstr);
            else if (kres == RdKafka::Conf::CONF_OK)
              break;

            /* Unknown property, fall through. */
          }

          if (!strncmp(name.c_str(), "kafka.", 6)) {
            RdKafka::Conf::ConfResult kres;

            kres = kconf->set(name.substr(6), val, errstr);
            if (kres == RdKafka::Conf::CONF_INVALID)
              FATAL(errstr);
            else if (kres == RdKafka::Conf::CONF_OK)
              break;

            /* Unknown property, fall through. */
          }

          /* Serdes config */
          Serdes::ErrorCode err = sconf->set(name, val, errstr);
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
        std::cerr << "% Unknown option -" << (char)opt << std::endl;
        usage(argv[0]);
    }
  }


  if (schema_name.empty()) {
    std::cerr << "% Missing argument -s <schema-name>" << std::endl;
    usage(argv[0]);
  }


  /* Create Avro Serdes handle */
  Serdes::Avro *serdes = Serdes::Avro::create(sconf, errstr);
  if (!serdes)
    FATAL("Failed to create Serdes handle: " << errstr);



  /**
   * Set up schema (either by getting existing or adding/updating)
   */
  int schema_id = -1;
  Serdes::Schema *schema;

  /* If schema name is an integer treat it as schema id. */
  if (!schema_name.empty() &&
      schema_name.find_first_not_of("0123456789") == std::string::npos) {
    schema_id = atoi(schema_name.c_str());
    schema_name.clear();
  }

  if (schema_def.empty()) {
    /* Query schema registry */

    std::cout << "% Query schema: by name \"" << schema_name << "\" or id "
              << schema_id << std::endl;

    if (!schema_name.empty())
      schema = Serdes::Schema::get(serdes, schema_name, errstr);
    else if (schema_id != -1)
      schema = Serdes::Schema::get(serdes, schema_id, errstr);
    else
      FATAL("Expected schema -s <id> or -s <name>");

    if (!schema)
      FATAL("Failed to get schema: " << errstr);

    std::cout << "% Schema \"" << schema->name() << "\" id " << schema->id() <<
        ": " << schema->definition() << std::endl;

  } else {
    /* Register new schema */

    std::cout << "% Register new schema: " << schema_name << ": "
              << schema_def << std::endl;

    schema = Serdes::Schema::add(serdes, schema_name,
                                 schema_def, errstr);

    if (!schema)
      FATAL("Failed to register schema " << schema_name << ": " << errstr);

    std::cout << "% Registered schema " << schema->name() << " with id "
              << schema->id() << std::endl;

  }

  if (topic.empty())
    exit(0);


  /* Set up a delivery report callback to track delivery status on a
   * per message basis */
  MyDeliveryReportCb dr_cb;
  if (kconf->set("dr_cb", &dr_cb, errstr) != RdKafka::Conf::CONF_OK)
    FATAL(errstr);

  /* Create Kafka producer */
  RdKafka::Producer *producer = RdKafka::Producer::create(kconf, errstr);
  if (!producer)
    FATAL(errstr);
  delete kconf;

  /* Create topic object */
  RdKafka::Topic *ktopic = RdKafka::Topic::create(producer, topic,
                                                  tconf, errstr);
  if (!ktopic)
    FATAL(errstr);
  delete tconf;

  /*
   * Read JSON from stdin, convert to Avro datum, serialize and produce
   */
  for (std::string line; run && std::getline(std::cin, line);) {
    avro::GenericDatum *datum = NULL;
    std::vector<char> out;

    /* Convert JSON to Avro object */
    if (json2avro(schema, line, &datum) == -1)
      continue;

    /* Serialize Avro */
    if (serdes->serialize(schema, datum, out, errstr) == -1) {
      std::cerr << "% Avro serialization failed: " << errstr << std::endl;
      delete datum;
      continue;
    }
    delete datum;

    /* Produce to Kafka */
    RdKafka::ErrorCode kerr = producer->produce(ktopic, partition,
                                                &out, NULL, NULL);

    if (kerr != RdKafka::ERR_NO_ERROR) {
      std::cerr << "% Failed to produce message: "
                << RdKafka::err2str(kerr) << std::endl;
      break;
    }
  }

  /* Wait for all messages to be delivered */
  while (producer->outq_len() > 0)
    producer->poll(100);

  delete producer;
  delete serdes;

  return 0;
}
