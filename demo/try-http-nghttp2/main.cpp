#include "Boot/Boot.h"
#include "Log/Log.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <nghttp2/nghttp2.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

/// HTTP response structure
struct HttpResponse {
    int status_code = 0;
    std::string headers;
    std::string body;
};

/// Helper function to establish SSL/TLS connection
SSL* setupTLS(int sockfd)
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
void cleanupTLS(SSL* ssl)
{
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_CTX* ctx = SSL_get_SSL_CTX(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
    }
}

/// Helper function to send HTTP request
bool sendHttpRequest(int sockfd, SSL* ssl, const std::string& request)
{
    if (ssl) {
        int ret = SSL_write(ssl, request.c_str(), request.size());
        if (ret <= 0) {
            Log::Error("Failed to send request over SSL");
            return false;
        }
    }
    else {
        if (send(sockfd, request.c_str(), request.size(), 0) < 0) {
            Log::Error("Failed to send request");
            return false;
        }
    }
    return true;
}

/// Helper function to receive HTTP response
std::string receiveHttpResponse(int sockfd, SSL* ssl)
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
        }
        else {
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
void parseHttpResponse(const std::string& raw_response, HttpResponse& response)
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
HttpResponse makeHttpRequest(const std::string& host, int port, const std::string& path, bool use_tls = false)
{
    HttpResponse response;

    // Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        Log::Error("Failed to create socket");
        return response;
    }

    // Resolve hostname
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        Log::Error("Failed to resolve hostname: {}", host);
        close(sockfd);
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
        close(sockfd);
        return response;
    }

    Log::Info("Connected to {}:{}", host, port);

    // Setup TLS if needed
    SSL* ssl = nullptr;
    if (use_tls) {
        ssl = setupTLS(sockfd);
        if (!ssl) {
            close(sockfd);
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
        close(sockfd);
        return response;
    }

    // Receive and parse response
    std::string raw_response = receiveHttpResponse(sockfd, ssl);
    parseHttpResponse(raw_response, response);

    // Cleanup
    cleanupTLS(ssl);
    close(sockfd);

    return response;
}

int main(int argc, const char** argv)
{
    Boot::LogHeader({argc, argv});

    // Display nghttp2 version
    nghttp2_info* info = nghttp2_version(0);
    Log::Info("nghttp2 version: {}", info->version_str);

    // Request 1: Plain HTTP to ifconfig.io
    Log::Info("========== Request 1: HTTP ifconfig.io ==========");
    HttpResponse resp1 = makeHttpRequest("ifconfig.io", 80, "/", false);
    Log::Info("===== Status Code: {}", resp1.status_code);
    Log::Info("===== Headers:");
    std::cout << resp1.headers << "\n";
    Log::Info("===== Body:");
    std::cout << resp1.body << "\n";

    // Request 2: HTTPS to httpbin.org
    Log::Info("========== Request 2: HTTPS httpbin.org /headers ==========");
    HttpResponse resp2 = makeHttpRequest("httpbin.org", 443, "/headers", true);
    Log::Info("===== Status Code: {}", resp2.status_code);
    Log::Info("===== Headers:");
    std::cout << resp2.headers << "\n";
    Log::Info("===== Body:");
    std::cout << resp2.body << "\n";

    return 0;
}
