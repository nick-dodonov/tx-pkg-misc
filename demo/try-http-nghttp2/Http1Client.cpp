#include "Http1Client.h"
#include "SocketCompat.h"
#include "Log/Log.h"
#include <cstring>
#include <openssl/err.h>
#include <openssl/ssl.h>

/// Helper function to establish SSL/TLS connection
static SSL* setupTLS(SOCKET sockfd)
{
    SSL_library_init();
    SSL_load_error_strings();

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        Log::Error("Failed to create SSL context");
        return nullptr;
    }

    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        Log::Error("Failed to create SSL object");
        SSL_CTX_free(ctx);
        return nullptr;
    }

    SSL_set_fd(ssl, sockfd);

    if (SSL_connect(ssl) <= 0) {
        Log::Error("Failed to establish SSL connection");
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return nullptr;
    }

    Log::Info("TLS connection established");
    return ssl;
}

/// Helper function to cleanup TLS
static void cleanupTLS(SSL* ssl)
{
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_CTX* ctx = SSL_get_SSL_CTX(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
    }
}

/// Helper function to send HTTP request
static bool sendHttpRequest(SOCKET sockfd, SSL* ssl, const std::string& request)
{
    if (ssl) {
        int ret = SSL_write(ssl, request.c_str(), request.size());
        if (ret <= 0) {
            Log::Error("Failed to send request over SSL");
            return false;
        }
    } else {
        if (send(sockfd, request.c_str(), request.size(), 0) < 0) {
            Log::Error("Failed to send request");
            return false;
        }
    }
    return true;
}

/// Helper function to receive HTTP response
static std::string receiveHttpResponse(SOCKET sockfd, SSL* ssl)
{
    std::string full_response;
    char buffer[4096];
    int nread;

    while (true) {
        if (ssl) {
            nread = SSL_read(ssl, buffer, sizeof(buffer));
            if (nread <= 0) {
                break;
            }
        } else {
            nread = recv(sockfd, buffer, sizeof(buffer), 0);
            if (nread <= 0) {
                break;
            }
        }
        full_response.append(buffer, nread);
    }

    return full_response;
}

/// Parse HTTP response to extract status code, headers and body
static void parseHttpResponse(const std::string& raw_response, HttpResponse& response)
{
    // Find the end of headers (either \r\n\r\n or \n\n)
    size_t header_end = raw_response.find("\r\n\r\n");
    size_t header_offset = 4;

    if (header_end == std::string::npos) {
        header_end = raw_response.find("\n\n");
        header_offset = 2;
    }

    std::string headers_part = (header_end != std::string::npos) ? raw_response.substr(0, header_end) : raw_response;

    response.headers = headers_part;
    if (header_end != std::string::npos) {
        response.body = raw_response.substr(header_end + header_offset);
    }

    // Extract status code from first line (HTTP/1.1 200 OK)
    size_t first_space = headers_part.find(' ');
    if (first_space != std::string::npos) {
        size_t second_space = headers_part.find(' ', first_space + 1);
        if (second_space != std::string::npos) {
            std::string status_str = headers_part.substr(first_space + 1, second_space - first_space - 1);
            response.status_code = std::stoi(status_str);
        }
    }
}

/// Make HTTP or HTTPS request to a server
HttpResponse makeHttpRequest(const std::string& host, int port, const std::string& path, bool use_tls)
{
    HttpResponse response;

    // Create socket
    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        Log::Error("Failed to create socket");
        return response;
    }

    // Resolve hostname
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        Log::Error("Failed to resolve hostname: {}", host);
        close_socket(sockfd);
        return response;
    }

    // Connect to server
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sockfd, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        Log::Error("Failed to connect to {}:{}", host, port);
        close_socket(sockfd);
        return response;
    }

    Log::Info("Connected to {}:{}", host, port);

    // Setup TLS if needed
    SSL* ssl = nullptr;
    if (use_tls) {
        ssl = setupTLS(sockfd);
        if (!ssl) {
            close_socket(sockfd);
            return response;
        }
    }

    // Build and send HTTP/1.1 request
    std::string request = "GET " + path +
                          " HTTP/1.1\r\n"
                          "Host: " +
                          host +
                          "\r\n"
                          "Connection: close\r\n"
                          "User-Agent: curl/8.7.1\r\n"
                          "\r\n";

    Log::Info("Sending HTTP request ({} bytes) to {}...", request.size(), host);

    if (!sendHttpRequest(sockfd, ssl, request)) {
        cleanupTLS(ssl);
        close_socket(sockfd);
        return response;
    }

    // Receive and parse response
    std::string raw_response = receiveHttpResponse(sockfd, ssl);
    parseHttpResponse(raw_response, response);

    // Cleanup
    cleanupTLS(ssl);
    close_socket(sockfd);

    return response;
}
