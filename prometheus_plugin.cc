#include <time.h>
#include <vector>

#include <trunk-recorder/source.h>
#include <trunk-recorder/plugin_manager/plugin_api.h>
#include <trunk-recorder/gr_blocks/decoder_wrapper.h>
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

protected:

public:
  Prometheus()
  {
  }

  void send_config(std::vector<Source *> sources, std::vector<System *> systems)
  {
    boost::property_tree::ptree root;
    boost::property_tree::ptree systems_node;
    boost::property_tree::ptree sources_node;

    for (std::vector<Source *>::iterator it = sources.begin(); it != sources.end(); it++)
    {
      Source *source = *it;
      std::vector<Gain_Stage_t> gain_stages;
      boost::property_tree::ptree source_node;
      source_node.put("source_num", source->get_num());
      source_node.put("antenna", source->get_antenna());

      source_node.put("silence_frames", source->get_silence_frames());

      source_node.put("min_hz", source->get_min_hz());
      source_node.put("max_hz", source->get_max_hz());
      source_node.put("center", source->get_center());
      source_node.put("rate", source->get_rate());
      source_node.put("driver", source->get_driver());
      source_node.put("device", source->get_device());
      source_node.put("error", source->get_error());
      source_node.put("gain", source->get_gain());
      gain_stages = source->get_gain_stages();
      for (std::vector<Gain_Stage_t>::iterator gain_it = gain_stages.begin(); gain_it != gain_stages.end(); gain_it++)
      {
        source_node.put(gain_it->stage_name + "_gain", gain_it->value);
      }
      source_node.put("antenna", source->get_antenna());
      source_node.put("analog_recorders", source->analog_recorder_count());
      source_node.put("digital_recorders", source->digital_recorder_count());
      source_node.put("debug_recorders", source->debug_recorder_count());
      source_node.put("sigmf_recorders", source->sigmf_recorder_count());
      sources_node.push_back(std::make_pair("", source_node));
    }

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *sys = (System *)*it;

      boost::property_tree::ptree sys_node;
      boost::property_tree::ptree channels_node;
      sys_node.put("audioArchive", sys->get_audio_archive());
      sys_node.put("systemType", sys->get_system_type());
      sys_node.put("shortName", sys->get_short_name());
      sys_node.put("sysNum", sys->get_sys_num());
      sys_node.put("uploadScript", sys->get_upload_script());
      sys_node.put("recordUnkown", sys->get_record_unknown());
      sys_node.put("callLog", sys->get_call_log());
      sys_node.put("talkgroupsFile", sys->get_talkgroups_file());
      sys_node.put("analog_levels", sys->get_analog_levels());
      sys_node.put("digital_levels", sys->get_digital_levels());
      sys_node.put("qpsk", sys->get_qpsk_mod());
      sys_node.put("squelch_db", sys->get_squelch_db());
      std::vector<double> channels;

      if ((sys->get_system_type() == "conventional") || (sys->get_system_type() == "conventionalP25"))
      {
        channels = sys->get_channels();
      }
      else
      {
        channels = sys->get_control_channels();
      }

      // std::cout << "starts: " << std::endl;

      for (std::vector<double>::iterator chan_it = channels.begin(); chan_it != channels.end(); chan_it++)
      {
        double channel = *chan_it;
        boost::property_tree::ptree channel_node;
        // std::cout << "Hello: " << channel << std::endl;
        channel_node.put("", channel);

        // Add this node to the list.
        channels_node.push_back(std::make_pair("", channel_node));
      }
      sys_node.add_child("channels", channels_node);

      if (sys->get_system_type() == "smartnet")
      {
        sys_node.put("bandplan", sys->get_bandplan());
        sys_node.put("bandfreq", sys->get_bandfreq());
        sys_node.put("bandplan_base", sys->get_bandplan_base());
        sys_node.put("bandplan_high", sys->get_bandplan_high());
        sys_node.put("bandplan_spacing", sys->get_bandplan_spacing());
        sys_node.put("bandplan_offset", sys->get_bandplan_offset());
      }
      systems_node.push_back(std::make_pair("", sys_node));
    }
    root.add_child("sources", sources_node);
    root.add_child("systems", systems_node);
    root.put("captureDir", this->config->capture_dir);
    root.put("uploadServer", this->config->upload_server);

    // root.put("defaultMode", default_mode);
    root.put("callTimeout", this->config->call_timeout);
    root.put("logFile", this->config->log_file);
    root.put("instanceId", this->config->instance_id);
    root.put("instanceKey", this->config->instance_key);
    root.put("logFile", this->config->log_file);
    root.put("type", "config");

    if (this->config->broadcast_signals == true)
    {
      root.put("broadcast_signals", this->config->broadcast_signals);
    }

    send_object(root, "config", "config", true);

    // Send the recorders in addition to the config, cause there isn't a better place to do it.
    std::vector<Recorder *> recorders;

    for (std::vector<Source *>::iterator it = sources.begin(); it != sources.end(); it++) {
      Source *source = *it;

      std::vector<Recorder *> sourceRecorders = source->get_recorders();

      recorders.insert(recorders.end(), sourceRecorders.begin(), sourceRecorders.end());
    }

    send_recorders(recorders);
  }

  int send_systems(std::vector<System *> systems)
  {

    boost::property_tree::ptree node;

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); it++)
    {
      System *system = *it;
      node.push_back(std::make_pair("", system->get_stats()));
    }
    return send_object(node, "systems", "systems", true);
  }

  int send_system(System *system)
  {

    return send_object(system->get_stats(), "system", "system");
  }

  int calls_active(std::vector<Call *> calls) override
  {

    boost::property_tree::ptree node;

    for (std::vector<Call *>::iterator it = calls.begin(); it != calls.end(); it++)
    {
      Call *call = *it;
      // if (call->get_state() == RECORDING) {
      node.push_back(std::make_pair("", call->get_stats()));
      //}
    }

    return send_object(node, "calls", "calls_active");
  }

  int send_recorders(std::vector<Recorder *> recorders)
  {

    boost::property_tree::ptree node;

    for (std::vector<Recorder *>::iterator it = recorders.begin(); it != recorders.end(); it++)
    {
      Recorder *recorder = *it;
      node.push_back(std::make_pair("", recorder->get_stats()));
    }

    return send_object(node, "recorders", "recorders",true);
  }

  int call_start(Call *call) override
  {

    return send_object(call->get_stats(), "call", "call_start");
  }

  int call_end(Call_Data_t call_info) override
  {

    return 0;
  }

  int send_recorder(Recorder *recorder)
  {

    return send_object(recorder->get_stats(), "recorder", "recorder");
  }

  int send_object(boost::property_tree::ptree data, std::string name, std::string type, bool retain = false)
  {
    this->http_requests_counter->Add({}).Increment();
    boost::property_tree::ptree root;
    root.add_child(name, data);
    root.put("type", type);

    std::stringstream stats_str;
    boost::property_tree::write_json(stats_str, root);

    return 0;
  }

  int poll_one() override
  {
    return 0;
  }

  int init(Config *config, std::vector<Source *> sources, std::vector<System *> systems) override
  {
    this->config = config;
    this->exposer = new Exposer(std::to_string(this->port));
    this->registry = std::make_shared<Registry>();

    this->http_requests_counter = &BuildCounter()
                                  .Name("http_requests_total")
                                  .Help("Number of HTTP requests")
                                  .Register(*registry);

    this->exposer->RegisterCollectable(this->registry);

    return 0;
  }

  int start() override
  {
    return 0;
  }

  int setup_recorder(Recorder *recorder) override
  {
    this->send_recorder(recorder);
    return 0;
  }

  int setup_system(System *system) override
  {
    this->send_system(system);
    return 0;
  }

  int setup_systems(std::vector<System *> systems) override
  {

    this->send_systems(systems);
    return 0;
  }

  int setup_config(std::vector<Source *> sources, std::vector<System *> systems) override
  {
    send_config(sources, systems);
    return 0;
  }

  // Factory method
  static boost::shared_ptr<Prometheus> create()
  {
    return boost::shared_ptr<Prometheus>(
        new Prometheus());
  }

  int parse_config(boost::property_tree::ptree &cfg) override
  {

    this->port = cfg.get<uint16_t>("port", 9842);
    BOOST_LOG_TRIVIAL(info) << " Prometheus Plugin Port: " << std::to_string(this->port);

    return 0;
  }

};

BOOST_DLL_ALIAS(
    Prometheus::create, // <-- this function is exported with...
    create_plugin        // <-- ...this alias name
)
