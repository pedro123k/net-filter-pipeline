#pragma once

#include <boost/asio.hpp>
#include <boost/endian/arithmetic.hpp>
#include <memory>
#include <array>
#include <bitset>
#include <chrono>
#include <unordered_map>
#include <nfp/SignalPipeline.hpp>
#include <thread>
#include <vector>

using boost::asio::ip::udp;
using std::chrono::steady_clock;

namespace nfp {
    #pragma pack(push, 1)
    struct Datagram{
        std::uint64_t seq;
        std::uint16_t out_port;
        float data[128];
    };
    #pragma pack(pop)

    enum class CONCEALMENT {REPEAT_LAST_GOOD, FADE_LAST_GOOD, ALL_ZERO};

    class UDPClient;
    class UDPServer;

    class UDPWorker { 
    private:
        static constexpr int W = 32;
        static constexpr auto default_timeout = std::chrono::seconds(10);
        static inline const std::vector<float> ZERO_OUTPUT = std::vector<float>(128, 0);

        CONCEALMENT loss_policy {CONCEALMENT::REPEAT_LAST_GOOD};

        struct ConnState {
            std::array<Datagram, W> buffer {};
            std::bitset<W> present {};
            uint64_t expected_seq = 0;
            bool initialized = false;
            std::vector<float> last_good = std::vector<float>(128, 0);
            uint16_t last_port = 55555;
            std::vector<float> faded_last_good = std::vector<float>(128, 0);

            steady_clock::time_point last_arrive = steady_clock::now();
            steady_clock::time_point deadline = steady_clock::time_point::max();
            SignalPipeline pipeline;

        };

        std::unordered_map<uint64_t, ConnState> conns;
        std::mutex conns_mtx;
        boost::asio::thread_pool& thread_pool;
        std::unique_ptr<UDPClient> client;

        boost::asio::steady_timer reap_timer;
        std::chrono::milliseconds reap_period {15000};

        void schedule_reap();
        void reap_dead_conns();

        
    public:
        UDPWorker(boost::asio::thread_pool& pool) : thread_pool(pool), reap_timer(pool.get_executor()) { this->schedule_reap(); }
        void handle_pkg(Datagram, udp::endpoint);
        void send_to_client(const std::vector<float>&, uint16_t);
        void set_client(std::unique_ptr<UDPClient> client) {this->client = std::move(client); }
        boost::asio::thread_pool& get_executor() { return this->thread_pool; }
        ~UDPWorker(){ this->reap_timer.cancel(); }
    };

    class UDPServer : public std::enable_shared_from_this<UDPServer> {
    private:
        udp::socket socket;
        udp::endpoint remote_endpoint;
        Datagram rcv_package;
        std::unique_ptr<UDPWorker> worker;

        void start_receive();

    public:
        UDPServer(boost::asio::io_context& io_context, int port) 
            : socket(io_context, udp::endpoint(udp::v4(), port)) {}

        void set_worker(std::unique_ptr<UDPWorker> w) {worker = std::move(w); }
        void start() { this->start_receive(); }
    }; 

    class UDPClient {
    private:
        boost::asio::ip::udp::socket socket;
        boost::asio::ip::address_v4 dest_ip;

    public:
        UDPClient(boost::asio::io_context & io_context, const boost::asio::ip::address_v4 out_addr) 
            : socket(io_context, udp::v4()), dest_ip(out_addr) {}

        void async_send(const std::vector<float>&, uint16_t);
    };
}