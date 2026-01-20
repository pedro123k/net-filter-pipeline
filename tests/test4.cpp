#include <nfp/ConfigsParse.hpp>
#include <iostream>
#include <vector>
#include <unordered_map>

using namespace std;
using namespace nfp;

int main(int argc, char ** argv) {

    if (argc < 2){
        cerr << "JSON config file is missing!" << endl;
        return -1;
    }

    json j = load_config_file(argv[1]);

    vector<PElement_info> filters = std::move(parse_pipeline_from(j));

    for (const auto & f: filters) {
        cout << "order: " << static_cast<int>(f.order) << endl;
        cout << "cutting frequency: " << f.cut_freq << endl;
        cout << "Q: " << f.Q << endl;
        cout << "BW: " << f.BW << endl;
        cout << "gain: " << f.gain << endl; 
        cout << "type: " << f.type << endl << endl;
    }

    Conn_info conn_info = std::move(parse_conn_from(j));

    unordered_map<CONCEALMENT, string> _t = {
        {CONCEALMENT::REPEAT_LAST_GOOD, "REPEAT_LAST_GOOD"},
        {CONCEALMENT::ALL_ZERO, "ALL_ZERO"},
        {CONCEALMENT::FADE_LAST_GOOD, "FADE_LAST_GOOD"}
    }; 

    cout << "Server Port: " << conn_info.server_port << endl;
    cout << "Sampling Frequency: " << conn_info.samp_freq << endl;
    cout << "Client Address (IPV4): " << conn_info.client_addrv4 << endl;
    cout << "Concealment Policy: " << _t[conn_info.policy] << endl;

    return 0;
}