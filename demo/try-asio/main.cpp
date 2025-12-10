#include "Boot/Boot.h"
#include "Log/Log.h"
#include <asio.hpp>
#include <array>
#include <ctime>

using asio::ip::tcp;

static std::string make_daytime_string()
{
    std::time_t now = time(0);
    return std::ctime(&now);
}

class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
public:
    typedef std::shared_ptr<tcp_connection> pointer;
    static pointer create(asio::io_context& io_context) { return pointer(new tcp_connection(io_context)); }

    tcp::socket& socket() { return socket_; }

    void start()
    {
        message_ = make_daytime_string();
        Log::Info(std::format("writing: {}", message_));
        asio::async_write(
            socket_,
            asio::buffer(message_),
            std::bind(
                &tcp_connection::handle_write,
                shared_from_this(),
                asio::placeholders::error,
                asio::placeholders::bytes_transferred
            )
        );
    }

private:
    tcp_connection(asio::io_context& io_context)
        : socket_(io_context)
    {
    }

    void handle_write(const std::error_code& error, size_t bytes_transferred)
    {
        auto err_msg = std::string(std::system_error(error).what());
        Log::Debug("written: {} bytes ({})", bytes_transferred, err_msg);
    }

    tcp::socket socket_;
    std::string message_;
};

class tcp_server {
public:
    tcp_server(asio::io_context& io_context)
        : io_context_(io_context)
        , acceptor_(io_context, tcp::endpoint(tcp::v4(), 13))
    {
        start_accept();
    }

private:
    void start_accept()
    {
        Log::Info("listening");
        tcp_connection::pointer new_connection = tcp_connection::create(io_context_);

        acceptor_.async_accept(
            new_connection->socket(),
            std::bind(&tcp_server::handle_accept, this, new_connection, asio::placeholders::error)
        );
    }

    void handle_accept(tcp_connection::pointer new_connection, const std::error_code& error)
    {
        Log::Info("accepted");
        if (!error) {
            new_connection->start();
        }

        start_accept();
    }

    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};

int main_server() {
//   try
//   {
    Log::Info("initializing");
    asio::io_context io_context;
    tcp_server server(io_context);
    io_context.run();
//   }
//   catch (std::exception& e)
//   {
//     std::cerr << e.what() << std::endl;
//   }
  return 0;
}

int main(int argc, const char** argv)
{
    Boot::LogHeader(argc, argv);

    // try {
        if (argc != 2) {
            Log::Error(std::format("Usage: {} {{-s|<host>}}", argv[0]));
            Log::Error("Service emulation: `while true; do echo -ne \"$(date -u)\\0\" | nc -l 13; echo accessed; done`");
            return 1;
        }

        auto arg = argv[1];
        if (strcmp(arg, "-s") == 0) {
            return main_server();
        }

        asio::io_context io_context;
        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve(argv[1], "daytime");
        tcp::socket socket(io_context);
        asio::connect(socket, endpoints);
        for (;;) {
            std::array<char, 128> buf;
            std::error_code error;

            size_t len = socket.read_some(asio::buffer(buf), error);
            if (error == asio::error::eof) {
                Log::Info("closed normally");
                break; // Connection closed cleanly by peer.
            } else if (error) {
                //throw std::system_error(error); // Some other error.
                Log::Error(std::format("closed w/ error: {}", error.message()));
                break;
            }

            Log::Info(std::format("Result: {}", std::string_view(buf.data(), len)));
        }
    // } catch(std::exception ex) {
    //     std::cerr << "EXCEPTION: " << ex.what();
    // }
    return 0;
}
