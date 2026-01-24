#include <nfp/ConfigsParse.hpp>
#include <stdexcept>
#include <fstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <cctype>
#include <algorithm>

json nfp::load_config_file(const std::string& fp) {
    std::ifstream f{fp};
    
    if (!f.is_open()) {
        throw std::runtime_error("JSON coundn't be parsed!");
    }

    try {
        json j;
        f >> j;
        return j;
    } catch (const json::parse_error& err) {
        throw std::runtime_error("Invalid JSON file (" + fp + ")" + ": " + err.what());
    }
}

static std::string lower_string(const std::string& str) {
    std::string lowerc_type = str;
    std::transform(lowerc_type.begin(), lowerc_type.end(), lowerc_type.begin(),
    [](unsigned char c) { return std::tolower(c); });
    return lowerc_type;
}

static std::string element_type_consistency(const std::string& type) {
    static const std::unordered_set<std::string> valid_types = {"low-pass", "high-pass", "band-pass", "notch", "gain"};
    
    auto lowerc_type = std::move(lower_string(type));
    auto it = valid_types.find(lowerc_type);

    if (it == valid_types.end())
        throw std::runtime_error(lowerc_type + " is not a valid element type!");

    return lowerc_type;

}

static bool requires_cutoff(const std::string& type) {
    static const std::unordered_set<std::string> req = {
        "low-pass", "high-pass", "band-pass", "notch"
    };

    return req.find(type) != req.end();
}

static bool requires_order(const std::string& type) {
    static const std::unordered_set<std::string> req = {
        "low-pass", "high-pass"
    };

    return req.find(type) != req.end();
}

static bool requires_bw(const std::string& type) {
    static const std::unordered_set<std::string> req = {
        "band-pass", "notch"
    };
    return req.find(type) != req.end();
}

static bool requires_gain(const std::string& type) {
    return lower_string(type) == "gain";
}

void nfp::from_json(const json& j, nfp::PElement_info& f_info) {
    if (!j.is_object()) 
        throw std::runtime_error("Filter must be a JSON Object!");

    if (!j.contains("type") || !j["type"].is_string())
        throw std::runtime_error("Element's type must be a string!");

    f_info.type = element_type_consistency(j["type"].get<std::string>());

    if (j.contains("gain")) {

        if (!j["gain"].is_number())
            throw std::runtime_error("Gain must be a number!");

        f_info.gain = j["gain"].get<float>();
    }

    if (j.contains("cut-freq")) {

        if (!j["cut-freq"].is_number())
            throw std::runtime_error("Filter's cutoff frequency must be a number!");

        auto cut_freq = j["cut-freq"].get<float>();

        if (cut_freq < 0.0f) 
            throw std::runtime_error("Filter's cutoff frequency must be a non-zero positive number!");

        f_info.cut_freq = cut_freq;
    }

    if (j.contains("order")) {
        if (!j["order"].is_number_unsigned()) 
            throw std::runtime_error("Filter's order must be a non-zero positive integer!");
    
        int order = j["order"].get<int>();

        if (order < 0 || order > 255 || (order % 2) != 0) 
            throw std::runtime_error("Filter's order must be a non-zero even integer < 255!");
        
        f_info.order = static_cast<uint8_t>(order);
    }

    if (j.contains("Q")) {
        if (!j["Q"].is_number()) 
            throw std::runtime_error("Filter's Q must be a number!");
        
        float Q = j["Q"].get<float>();

        if (Q < 0) 
            throw std::runtime_error("Filter's order must be a non-zero number!");
        

        f_info.Q = Q;
    }

    if (j.contains("BW")) {
        if (!j["BW"].is_number())
            throw std::runtime_error("Filter's BW must be a number!");

        float bw = j["BW"].get<float>();
        if (bw < 0) 
            throw std::runtime_error("Filter's BW must be a non-zero number!");
        f_info.BW = bw;
    }

    if (requires_order(f_info.type) && !j.contains("order"))
        throw std::runtime_error("Filter " + f_info.type + " must have an order!");

    if (requires_bw(f_info.type) && !j.contains("BW"))
        throw std::runtime_error("Filter " + f_info.type + " must have a BW!");

    if (requires_gain(f_info.type) && !j.contains("gain"))
        throw std::runtime_error("Element " + f_info.type + " must have a gain!");

    if (requires_cutoff(f_info.type) && !j.contains("cut-freq"))
        throw std::runtime_error("Filter " + f_info.type + " must have a cutoff frequency!");

}

std::vector<nfp::PElement_info> nfp::parse_pipeline_from(const json& j) {
    const json arr = j.value("pipeline", json::array());

    if (!arr.is_array())
        throw std::runtime_error("Pipeline must be an array!");
        
    std::vector<nfp::PElement_info> out;
    out.reserve(arr.size());

    for (int i = 0; i < arr.size(); i++) {
        try {
            out.push_back(arr.at(i).get<PElement_info>());
        } catch (const std::exception& err) {
            throw std::runtime_error("Element Error[" + std::to_string(i)+ "]: " + std::string(err.what()));
        }
    }

    return out;
}

static nfp::CONCEALMENT concealment_policy_chk(std::string policy) {
    static const std::unordered_map<std::string, nfp::CONCEALMENT> conv = {
        {"REPEAT_LAST_GOOD", nfp::CONCEALMENT::REPEAT_LAST_GOOD},
        {"FADE_LAST_GOOD", nfp::CONCEALMENT::FADE_LAST_GOOD},
        {"ALL_ZERO", nfp::CONCEALMENT::ALL_ZERO}
    };

    auto it = conv.find(policy);

    if (it == conv.end())
        throw std::runtime_error(policy + " is not a valid concealment policy!");

    return it->second;    
}

void nfp::from_json(const json& j, nfp::Conn_info& conn_info) {

    if (!j.contains("server-port") || !j["server-port"].is_number_unsigned())
            throw std::runtime_error("Server must have a non-zero integer port!");

    conn_info.server_port = j["server-port"].get<uint16_t>();

    if (!j.contains("samp-freq") || !j["samp-freq"].is_number())
        throw std::runtime_error("Server must have a sampling frequency number!");

    float samp_freq = j["samp-freq"].get<float>();

    if (samp_freq <= 0.0f) 
        throw std::runtime_error("Sampling frequency must be a non-zero positive number!");

    conn_info.samp_freq = samp_freq;

    if (!j.contains("client-addrv4") || !j["client-addrv4"].is_string())
        throw std::runtime_error("Server must have a string client IPV4 address!");

    conn_info.client_addrv4 = j["client-addrv4"].get<std::string>();

    if (!j.contains("concealment-policy") || !j["concealment-policy"].is_string())
        throw std::runtime_error("Server must have a string concealment policy!");

    conn_info.policy = concealment_policy_chk(j["concealment-policy"]);
    
}

nfp::Conn_info nfp::parse_conn_from(const json& j) {
    const json obj = j.value("udp-parms", json::object());

    if (!obj.is_object())
        throw std::runtime_error("Connection's parameters must be a JSON Object!");

    try {
        return obj.get<nfp::Conn_info>();
    } catch (const std::exception& err) {
        throw std::runtime_error("UDP Parameters error: " + std::string(err.what()));
    }
}

nfp::SignalPipeline nfp::build_pipeline(const std::vector<PElement_info>& pelements, float fs) {
    nfp::SignalPipeline pipeline;

    enum _Elements{GAIN, LOW_PASS, HIGH_PASS, BAND_PASS, NOTCH}; 
    
    static const std::unordered_map<std::string, _Elements> strtype2enum = {
        {"gain", GAIN}, {"low-pass", LOW_PASS}, {"high-pass", HIGH_PASS}, {"band-pass", BAND_PASS}, {"notch", NOTCH}     
    };

    constexpr float PI = 3.14156f;

    auto to_rad = [PI](const float _fs, const float _fc) {return 2 * PI * (_fc / _fs); };

    for (const auto& pelement: pelements) {
        switch (strtype2enum.at(pelement.type)) {
            case GAIN:
                pipeline.add_gain(pelement.gain);
            break;
            case LOW_PASS:
                pipeline.add_digital_filter(DigitalFilter::low_pass_filter(
                    to_rad(fs, pelement.cut_freq),
                    pelement.order,
                    pelement.Q
                ));
                break;
            case HIGH_PASS:
                pipeline.add_digital_filter(DigitalFilter::high_pass_filter(
                    to_rad(fs, pelement.cut_freq),
                    pelement.order,
                    pelement.Q
                ));
                break;
            case BAND_PASS:
                pipeline.add_digital_filter(DigitalFilter::band_pass_filter(
                    to_rad(fs, pelement.cut_freq),
                    pelement.BW
                ));
                break;
            case NOTCH:
                pipeline.add_digital_filter(DigitalFilter::notch_filter(
                    to_rad(fs, pelement.cut_freq),
                    pelement.BW
                ));
                break;
        
        default:
            break;
        }        
    }

    return pipeline;
}  