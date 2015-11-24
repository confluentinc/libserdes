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
#include <stdio.h>


#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <getopt.h>

#include <netinet/in.h> // remove

/* Typical include path is <libserdes/serdescpp-avro.h> */
#include "../src-cpp/serdescpp-avro.h"

#include <librdkafka/rdkafkacpp.h>

#include <avro/Encoder.hh>
#include <avro/Decoder.hh>
#include <avro/Generic.hh>
#include <avro/Specific.hh>
#include <avro/Exception.hh>



static bool run = true;
static int verbosity = 2;
static long long msg_cnt = 0;
static long long msg_bytes = 0;
static Serdes::Avro *serdes;
static int payload_serialized = 0;
static int key_serialized = 0;


#define FATAL(reason...) do {                           \
    std::cerr << "% FATAL: " << reason << std::endl;      \
    exit(1);                                            \
  } while (0)



/**
 * Format Avro datum as JSON according to schema.
 */
static int avro2json (Serdes::Schema *schema, const avro::GenericDatum *datum,
                      std::string &str, std::string &errstr) {
  avro::ValidSchema *avro_schema = schema->object();

  /* JSON encoder */
  avro::EncoderPtr json_encoder = avro::jsonEncoder(*avro_schema);

  /* JSON output stream */
  std::ostringstream oss;
  std::auto_ptr<avro::OutputStream> json_os = avro::ostreamOutputStream(oss);

  try {
    /* Encode Avro datum to JSON */
    json_encoder->init(*json_os.get());
    avro::encode(*json_encoder, *datum);
    json_encoder->flush();

  } catch (const avro::Exception &e) {
    errstr = std::string("Binary to JSON transformation failed: ") + e.what();
    return -1;
  }

  str = oss.str();
  return 0;
}



/**
 * Deserialize/decode a value and print it to stdout as JSON fields with
 * the given prefix
 */
static void decode_and_print (const std::string &pfx,
                              const void *buf, size_t len) {
  std::string out;
  avro::GenericDatum *d = NULL;
  Serdes::Schema *schema = NULL;
  std::string errstr;

  if (serdes->deserialize(&schema, &d, buf, len, errstr) == -1 ||
      avro2json(schema, d, out, errstr) == -1) {
    std::cout << "\"" << pfx << "_error\": \"" << errstr << "\", ";

    /* Output raw string on deserialization failure. */
    std::cout << "\"" << pfx << "_len\": " << (int)len << ", ";
    out = std::string((const char *)buf, len);
  }

  std::cout << "\"" << pfx << "\": \"" << out << "\", ";

  if (d)
    delete d;
}


/**
 * Handle consumed message.
 * Will be either a proper message or an event/error.
 */
static void msg_handle (RdKafka::Message *msg) {
  switch (msg->err()) {
    case RdKafka::ERR__TIMED_OUT:
      break;

    case RdKafka::ERR_NO_ERROR:
      /* Proper message */
      msg_cnt++;
      msg_bytes += msg->len();

      /* Construct a JSON envelope.
       * FIXME: There is no escaping of field values. */
      std::cout << "{ ";

      if (msg->key()) {
        /* Decode Key */
        if (key_serialized)
          decode_and_print("key", msg->key()->c_str(), msg->key_len());
        else
          std::cout << "\"key\": \"" << msg->key() << "\", "
                    << "\"key_len\": " << (int)msg->key_len() << ", ";
      }

      if (msg->payload()) {
        /* Decode payload */
        if (payload_serialized)
          decode_and_print("payload", msg->payload(), msg->len());
        else
          std::cout << "\"payload\": \"" << (const char *)msg->payload() << "\", "
                    << "\"payload_len\": " << (int)msg->len() << ", ";
      }

      std::cout << "\"topic\": \"" << msg->topic_name() << "\", "
                << "\"partition\": " << msg->partition() << ", "
                << "\"offset\": " << msg->offset()
                << " }" << std::endl;
      break;

    case RdKafka::ERR__PARTITION_EOF:
      /* Last message */
      if (verbosity >= 3)
        std::cerr << "%% EOF reached for "<< msg->topic_name()
                  << " [" << (int)msg->partition() << "]" << std::endl;
      break;

    case RdKafka::ERR__UNKNOWN_TOPIC:
    case RdKafka::ERR__UNKNOWN_PARTITION:
      std::cerr << "Consume failed: " << msg->errstr() << std::endl;
      run = false;
      break;

    default:
      /* Errors */
      std::cerr << "Consume failed: " << msg->errstr() << std::endl;
      run = false;
  }

}



static __attribute__((noreturn))
void usage (const std::string me) {

  std::cerr <<
      "Usage: " << me << " [options] <topics1 topic2 ..>]\n"
      "\n"
      "Options:\n"
      " -b <brokers..>    Kafka broker(s)\n"
      " -g <group-id>     Consumer group id\n"
      " -r <schreg-urls>  Schema registry URL\n"
      " -j <json blob>    JSON blob to encode or decode\n"
      " -D key            Deserialize key (else print verbatim)\n"
      " -D payload        Deserialize payload (else print verbatim)\n"
      " -X kafka.topic.<n>=<v> Set RdKafka topic configuration\n"
      " -X kafka.<n>=<v>  Set RdKafka global configuration\n"
      " -X <n>=<v>        Set Serdes configuration\n"
      " -v                Increase verbosity\n"
      " -q                Decrease verbosity\n";

  exit(1);
}

static void sig_term (int sig) {
  run = false;
}


int main (int argc, char **argv) {
  std::string errstr;

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

  /* Default framing to CP1 */
  if (sconf->set("deserializer.framing", "cp1", errstr))
    FATAL("Conf failed: " << errstr);

  /* Create rdkafka configuration object.
   * Configured passed through -X kafka.prop=val will be set on this object. */
  RdKafka::Conf *kconf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

  /* Create rdkafka default topic configuration object.
   * Configuration passed through -X kafka.topic.prop=val will be set .. */
  RdKafka::Conf *tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);


  /* Command line argument parsing */
  int opt;
  while ((opt = getopt(argc, argv, "b:g:r:X:vqD:")) != -1) {
    switch (opt)
    {
      case 'b':
        if (kconf->set("bootstrap.servers", optarg, errstr) !=
            RdKafka::Conf::CONF_OK)
          FATAL(errstr);
        break;

      case 'g':
        if (kconf->set("group.id", optarg, errstr) != RdKafka::Conf::CONF_OK)
          FATAL(errstr);
        break;

      case 'r':
        if (sconf->set("schema.registry.url", optarg, errstr) != SERDES_ERR_OK)
          FATAL("Failed to set registry.url: " << errstr);
        break;

      case 'D':
        if (strstr(optarg, "key"))
          key_serialized = 1;
        if (strstr(optarg, "payload"))
          payload_serialized = 1;
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

  /* Remaining arguments are topics to subscribe to */
  if (optind == argc) {
    std::cerr << "% No topics to subscribe to" << std::endl;
    usage(argv[0]);
  }


  /* Create Avro Serdes handle */
  serdes = Serdes::Avro::create(sconf, errstr);
  if (!serdes)
    FATAL("Failed to create Serdes handle: " << errstr);
  delete sconf;


  /* Set default Kafka topic config */
  if (kconf->set("default_topic_conf", tconf, errstr) != RdKafka::Conf::CONF_OK)
    FATAL(errstr);
  delete tconf;

  /* Create Kafka consumer */
  RdKafka::KafkaConsumer *consumer = RdKafka::KafkaConsumer::create(kconf,
                                                                    errstr);
  if (!consumer)
    FATAL(errstr);
  delete kconf;

  /* Collect topics */
  std::vector<std::string> topics;
  topics.reserve(argc-optind);
  for ( ; optind < argc ; optind++) {
    std::cerr << "% Subscribe to topic " << argv[optind] << std::endl;
    topics.push_back(argv[optind]);
  }

  RdKafka::ErrorCode kerr = consumer->subscribe(topics);
  if (kerr != RdKafka::ERR_NO_ERROR)
    FATAL("Subscribe failed: " << RdKafka::err2str(kerr));


  /**
   * Consume messages
   */
  while (run) {
    /* Consume one message (or event) */
    RdKafka::Message *msg = consumer->consume(1000);
    msg_handle(msg);
    delete msg;
  }


  consumer->close();

  delete consumer;
  delete serdes;

  return 0;
}
