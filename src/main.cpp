#include <csignal>
#include <chrono>
#include <thread>
#include <iostream>
#include <nfp/UDPInterface.hpp>
#include <nfp/ConfigsParse.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <fstream>

#ifdef _WIN32
    #include <windows.h>
#endif

static volatile std::sig_atomic_t running = 1;

#ifdef _WIN32

BOOL WINAPI console_ctrl_handle(DWORD type) {
    switch (type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            running = 0;
            return TRUE;        
        default:
            return FALSE;
    }
}

#endif

extern "C" void on_sigint(int) {
    running = 0;
}

void setup_signals() {
    #ifdef _WIN32
        SetConsoleCtrlHandler(console_ctrl_handle, TRUE);
    #else
        std::signal(SIGINT, on_sigint);
    #endif
}

static const char* get_coeffs_save_path(int argc, char ** argv) {
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--dump-coeffs" && i + 1 < argc)
            return argv[i+1];
    }
    return nullptr;
}

int run_app(const std::string& json_path, const char * coeffs_save_path) {
    json j = nfp::load_config_file(json_path.c_str());

    nfp::Conn_info conn_info = nfp::parse_conn_from(j);
    nfp::SignalPipeline pipeline = nfp::build_pipeline(nfp::parse_pipeline_from(j), conn_info.samp_freq);

    if (coeffs_save_path) {
        auto file = std::ofstream(coeffs_save_path) ;

        if (!file){
            std::cerr << coeffs_save_path << " is not a proper path!" << std::endl;
        } else
            for (const auto& coeff : pipeline.coeffs())
                file << coeff << '\n';
    }

    boost::asio::io_context server_io, client_io;

    auto server_guard = boost::asio::make_work_guard(server_io);
    auto client_guard = boost::asio::make_work_guard(client_io);

    std::cout << "Initializing Server and Client... [Press CTRL + C to exit]" << std::endl;

    auto server = std::make_shared<nfp::UDPServer>(server_io, conn_info.server_port);
    auto client = std::make_unique<UDPClient>(client_io, boost::asio::ip::make_address_v4(conn_info.client_addrv4));

    boost::asio::thread_pool workers {2};

    auto worker = std::make_unique<nfp::UDPWorker>(workers);
    worker->set_client(std::move(client));
    worker->set_pipeline_factory([j, conn_info]() mutable {
        return nfp::build_pipeline(nfp::parse_pipeline_from(j), conn_info.samp_freq);
    });
    worker->set_concealment_policy(conn_info.policy);
    server->set_worker(std::move(worker));
    server->start();

    std::thread server_thread ([&]() { server_io.run(); });
    std::thread client_thread ([&]() { client_io.run(); });

    while (running) 
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    server->finish();

    server_guard.reset();
    client_guard.reset();

    server_io.stop();
    client_io.stop();
    
    server_thread.join();
    client_thread.join();

    workers.stop();
    workers.join();

    return 0 ; 
}

int main(int argc, char ** argv) {

    if (argc < 2) {
        std::cerr << "ERROR: JSON config file is missing!";
        return -1;
    }

    setup_signals();

    try {
        return run_app(argv[1], get_coeffs_save_path(argc, argv));
    } catch (std::exception& err) {
        std::cerr << "ERROR: " << err.what() << std::endl; 
        return -1;
    }

}