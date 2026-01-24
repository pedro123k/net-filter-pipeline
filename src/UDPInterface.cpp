#include <nfp/UDPInterface.hpp>
#include <vector>
#include <algorithm>

using boost::asio::ip::udp;
using nfp::Datagram;
using nfp::UDPServer;
using nfp::UDPClient;
using nfp::UDPWorker;

void UDPServer::start_receive() {
    auto self = shared_from_this();

    this->socket.async_receive_from(
        boost::asio::buffer(&this->rcv_package, sizeof(Datagram)),
        this->remote_endpoint,
        [self](boost::system::error_code ec, std::size_t bytes_recv) {

            if (ec == boost::asio::error::operation_aborted) return;

            if (!ec && bytes_recv == sizeof(Datagram)){
                auto from = self->remote_endpoint;
                auto pkg = self->rcv_package;

                boost::asio::post(self->worker->get_executor(), [self, pkg = std::move(pkg), from = std::move(from) ](){
                    self->worker->handle_pkg(std::move(pkg), std::move(from));
                });
            }
            
            self->start_receive();
        }
    );
}

void UDPWorker::handle_pkg(Datagram pkg, udp::endpoint src) {

    uint64_t _hash = (static_cast<uint64_t>(src.address().to_v4().to_uint()) << 16) |
    static_cast<uint64_t>(src.port());

    uint16_t client_port;
    std::vector<float> client_output;
    std::vector<float> input;
    input.reserve(128);

    bool send_data = false;

    std::unique_lock<std::mutex> lock(this->conns_mtx);
    
    auto& conn = this->conns[_hash];

    if (!conn.is_ready) {
        conn.is_ready = true;
        conn.pipeline = this->pipeline_factory();
    }

    auto now = steady_clock::now();

    conn.last_arrive = now;
    conn.deadline = now + this->default_timeout;

    if (pkg.seq >= conn.expected_seq + 2 * W) {
        conn.expected_seq = pkg.seq;
        conn.present.reset();
    }

    int idx_new = pkg.seq % this->W;
    conn.buffer[idx_new] = pkg;
    conn.present[idx_new] = true;

    if (conn.present.count() < 5)
        conn.initialized = false;
    else
        conn.initialized = true;

    if (conn.initialized) {
        int idx = conn.expected_seq % this->W;
        
        if (conn.present[idx]) {

            float * ptr_data = conn.buffer[idx].data;            
            client_port = conn.buffer[idx].out_port;
            conn.last_port = client_port;
            
            input.assign(ptr_data, ptr_data + 128);
            conn.present[idx] = false;
            
            conn.last_good = input;
            conn.faded_last_good.resize(conn.last_good.size());
            
            for (size_t i = 0; i < conn.last_good.size(); ++i)
                conn.faded_last_good[i] = 0.8f * conn.last_good[i];

            send_data = true;
            ++conn.expected_seq;

        } else {

            std::vector<float> faded_last; 
            client_port = conn.last_port;

            switch (this->loss_policy) {
                case CONCEALMENT::REPEAT_LAST_GOOD:
                    input = conn.last_good;
                    break;
                case CONCEALMENT::FADE_LAST_GOOD:
                    input = conn.faded_last_good;

                    conn.faded_last_good.resize(conn.last_good.size());
                    for (size_t i = 0; i < conn.last_good.size(); ++i)
                        conn.faded_last_good[i] = 0.8f * conn.last_good[i];
                        
                    break;
                case CONCEALMENT::ALL_ZERO:
                    input = ZERO_OUTPUT;
                    break;  
                default:
                    break;
            }

            send_data = true;
            ++conn.expected_seq;
        }

    } else {
        client_port = conn.last_port;
        input = ZERO_OUTPUT;
        send_data = true;
        ++conn.expected_seq;
    }

    if (input.size() != 128)
        input = ZERO_OUTPUT;

    conn.pipeline.processBlock(input, client_output);

    lock.unlock();

    if (send_data)
        this->send_to_client(client_output, client_port);
}

void UDPWorker::send_to_client(const std::vector<float>& out, uint16_t port) {
    if (!this->client)
        return;
    
    auto data = std::make_shared<std::vector<float>>(out);
    
    boost::asio::post(this->thread_pool, [this, data, port]() {
        client->async_send(*data, port);
    });
}

void UDPClient::async_send(const std::vector<float>& data, uint16_t port) {
    udp::endpoint endp(this->dest_ip, port);

    this->socket.async_send_to(
        boost::asio::buffer(data.data(), data.size() * sizeof(float)),
        endp,
        [](boost::system::error_code ignored_ec, std::size_t l) { }
    );
}

void UDPWorker::schedule_reap() {
    this->reap_timer.expires_after(this->reap_period);

    this->reap_timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
            return;
        
        if (ec)
            return;

        this->reap_dead_conns();
        this->schedule_reap();
    });
}

void UDPWorker::reap_dead_conns() {
    const auto now = steady_clock::now();
    std::vector<uint64_t> addrs_to_remove;
    addrs_to_remove.reserve(this->conns.size());

    std::lock_guard<std::mutex> lock(this->conns_mtx);

    for (auto it = this->conns.begin(); it != this->conns.end(); ) {
        if (it->second.deadline <= now)
            it = this->conns.erase(it);
        else
            ++it;
    }
}

void nfp::UDPServer::finish() {
    if (this->worker)
        worker->stop();

    boost::system::error_code ec;
    this->socket.cancel(ec);
    this->socket.close(ec);    
}

void nfp::UDPClient::close() {
    boost::system::error_code ec;
    this->socket.cancel(ec);
    this->socket.close(ec);
}

void nfp::UDPWorker::stop() {
    if (this->client) 
        this->client->close();
    
    this->reap_timer.cancel(); 
}

