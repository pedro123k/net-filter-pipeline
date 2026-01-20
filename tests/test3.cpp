#include <boost/asio.hpp>
#include <iostream>
#include <array>
#include <nfp/SignalPipeline.hpp>
#include <nfp/UDPInterface.hpp>
#include <nfp/DigitalFilter.hpp>
#include <thread>
#include <memory>
#include <chrono>

using nfp::SignalPipeline;
using nfp::DigitalFilter;
using nfp::UDPServer;
using nfp::UDPWorker;
using nfp::UDPClient;

const float PI = 3.14156f;

int main() {
    try {

        float fs = 10000.0f;
        float fc = 1000.0f;

        float w0 = 2 * PI * (fc / fs);

        boost::asio::io_context server_io, client_io;

        auto server_guard = boost::asio::make_work_guard(server_io);
        auto client_guard = boost::asio::make_work_guard(client_io);        

        std::cout << "Initializing Server and Client..." << std::endl;
        auto server = std::make_shared<UDPServer>(server_io, 55555);
        std::unique_ptr<UDPClient> client = std::make_unique<UDPClient>(client_io, boost::asio::ip::make_address_v4("127.0.0.1"));
        
        SignalPipeline pipeline{};
        pipeline.add_digital_filter(DigitalFilter::low_pass_filter(w0));

        boost::asio::thread_pool workers {2};
        
        auto worker = std::make_unique<UDPWorker>(workers);
        worker->set_client(std::move(client));
        server->set_worker(std::move(worker));
        

        server->start();

        std::thread server_thread( [&]() {server_io.run(); } );

        std::thread client_thread ( [&]() {client_io.run(); } );

        std::cout << "Wait for response" << std::endl;

        while(true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        server_guard.reset();
        client_guard.reset();
        server_io.stop();
        client_io.stop();

        server_thread.join();
        client_thread.join();
        workers.join();
        
    } catch (std::exception & e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}