#pragma once

#include <nfp/SignalPipeline.hpp>
#include <nfp/UDPInterface.hpp>
#include <nfp/DigitalFilter.hpp>
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;
using nfp::SignalPipeline;
using nfp::UDPServer;
using nfp::UDPClient;

namespace nfp {    

    struct PElement_info {
        uint8_t order;
        float cut_freq;        
        float Q = 0.707f;
        float BW;
        float gain;
        std::string type;
    };

    struct Conn_info {
        float samp_freq;
        uint16_t server_port;
        std::string client_addrv4;
        nfp::CONCEALMENT policy;
    };

    json load_config_file(const std::string&);
    void from_json(const json&, PElement_info&);
    std::vector<PElement_info> parse_pipeline_from(const json&);
    void from_json(const json&, Conn_info&);
    Conn_info parse_conn_from(const json&);
}

