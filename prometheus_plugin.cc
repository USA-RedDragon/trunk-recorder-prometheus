#include <time.h>
#include <vector>

#include "trunk-recorder/source.h"
#include "trunk-recorder/plugin_manager/plugin_api.h"
#include "trunk-recorder/gr_blocks/decoder_wrapper.h"
#include <boost/dll/alias.hpp> // for BOOST_DLL_ALIAS
#include <boost/property_tree/json_parser.hpp>
#include <boost/log/trivial.hpp>

#include <iostream>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include <cstring>

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

typedef struct prometheus_plugin_t prometheus_plugin_t;

struct prometheus_plugin_t
{
  std::vector<Source *> sources;
  std::vector<System *> systems;
  std::vector<Call *> calls;
  Config *config;
};

using namespace std;
using namespace prometheus;

class Prometheus : public Plugin_Api
{
  std::vector<Call *> calls;
  Config *config;
  uint16_t port;
  prometheus::Exposer *exposer;
  std::shared_ptr<prometheus::Registry> registry;

  prometheus::Family<prometheus::Counter> *http_requests_counter;
  prometheus::Family<prometheus::Gauge> *active_calls;
  prometheus::Family<prometheus::Counter> *calls_counter;
  prometheus::Family<prometheus::Counter> *message_counter;

public:
  // Factory method
  static boost::shared_ptr<Prometheus> create()
  {
    return boost::shared_ptr<Prometheus>(new Prometheus());
  }

  int parse_config(boost::property_tree::ptree &cfg) override
  {
    this->port = cfg.get<uint16_t>("port", 9842);
    BOOST_LOG_TRIVIAL(info) << " Prometheus Plugin Port: " << std::to_string(this->port);

    return 0;
  }

  int init(Config *config, std::vector<Source *> sources, std::vector<System *> systems) override
  {
    this->config = config;
    this->exposer = new Exposer(std::to_string(this->port));
    this->registry = std::make_shared<Registry>();

    this->active_calls = &BuildGauge()
                          .Name("active_calls")
                          .Help("Number of active calls")
                          .Register(*registry);

    this->calls_counter = &BuildCounter()
                          .Name("calls")
                          .Help("Call history")
                          .Register(*registry);

    this->http_requests_counter = &BuildCounter()
                                  .Name("http_requests_total")
                                  .Help("Number of HTTP requests served")
                                  .Register(*registry);

    this->message_counter = &BuildCounter()
                              .Name("message_decodes")
                              .Help("Message decode count")
                              .Register(*registry);

    this->exposer->RegisterCollectable(this->registry);

    return 0;
  }

  int start() override
  {
    return 0;
  }

  int calls_active(std::vector<Call *> calls) override
  {
    std::map<std::string, int> callsPerSystem;

    auto ret = 0;
    for (std::vector<Call *>::iterator it = calls.begin(); it != calls.end(); it++)
    {
      Call *call = *it;

      std::string callSystem = call->get_system()->get_short_name();
      if (callsPerSystem.find(callSystem) == callsPerSystem.end())
      {
        callsPerSystem[callSystem] = 1;
      }
      else
      {
        callsPerSystem[callSystem] += 1;
      }

      this->calls_counter->Add({
        {"call_system", callSystem},
        {"encrypted", std::to_string(call->get_encrypted())},
        {"talkgroup", std::to_string(call->get_talkgroup())},
      }).Increment();

      ret = this->update_call_metrics(call);
      if (ret != 0)
      {
        return ret;
      }
    }

    for (auto& callPerSystem : callsPerSystem)
    {
      this->active_calls->Add({
        {"system", callPerSystem.first},
      }).Set(callPerSystem.second);
    }
    return ret;
  }

  int call_start(Call *call) override
  {
    return this->update_call_metrics(call);
  }

  int call_end(Call_Data_t call_info) override
  {
    return 0;
  }

  int setup_recorder(Recorder *recorder) override
  {
    return this->update_recorder_metrics(recorder);
  }

  int setup_config(std::vector<Source *> sources, std::vector<System *> systems) override
  {
    auto ret = 0;
    for (std::vector<Source *>::iterator it = sources.begin(); it != sources.end(); it++)
    {
      Source *source = *it;
      ret = this->update_source_metrics(source);
      if (ret != 0)
      {
        return ret;
      }
    }
    return ret;
  }

  int system_rates(std::vector<System *> systems, float timeDiff) override
  {
    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); it++)
    {
      System *system = *it;
      this->message_counter->Add({
        {"system", system->get_short_name()}
      }).Increment(system->get_message_count());
    }

    return 0;
  }

protected:

  int update_call_metrics(Call * call) {
    this->http_requests_counter->Add({}).Increment();
    BOOST_LOG_TRIVIAL(info) << "Updating call metrics";
    return 0;
  }

  int update_recorder_metrics(Recorder * recorder) {
    this->http_requests_counter->Add({}).Increment();
    BOOST_LOG_TRIVIAL(info) << "Updating recorder metrics";
    return 0;
  }

  int update_source_metrics(Source * sources) {
    this->http_requests_counter->Add({}).Increment();
    BOOST_LOG_TRIVIAL(info) << "Updating source metrics";
    return 0;
  }
};

BOOST_DLL_ALIAS(
    Prometheus::create, // <-- this function is exported with...
    create_plugin        // <-- ...this alias name
)
