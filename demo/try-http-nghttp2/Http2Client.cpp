#include "Http2Client.h"
#include "SocketCompat.h"
#include "Log/Log.h"
#include <cstring>
#include <nghttp2/nghttp2.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

/// HTTP/2 session data
struct Http2SessionData
{
    nghttp2_session* session = nullptr;
    SSL* ssl = nullptr;
    SOCKET sockfd = INVALID_SOCKET;
    HttpResponse response;
    bool response_complete = false;
};

/// Callback: Send data to the remote peer
static ssize_t send_callback(nghttp2_session* session, const uint8_t* data, size_t length, int flags, void* user_data)
{
    Http2SessionData* session_data = static_cast<Http2SessionData*>(user_data);
    int rv = SSL_write(session_data->ssl, data, length);
    if (rv <= 0) {
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    return rv;
}

/// Callback: Receive header name/value pair
static int on_header_callback(
    nghttp2_session* session,
    const nghttp2_frame* frame,
    const uint8_t* name,
    size_t namelen,
    const uint8_t* value,
    size_t valuelen,
    uint8_t flags,
    void* user_data
)
{
    Http2SessionData* session_data = static_cast<Http2SessionData*>(user_data);

    if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
        std::string name_str(reinterpret_cast<const char*>(name), namelen);
        std::string value_str(reinterpret_cast<const char*>(value), valuelen);

        session_data->response.headers += name_str + ": " + value_str + "\n";

        // Extract status code from :status header
        if (name_str == ":status") {
            session_data->response.status_code = std::stoi(value_str);
        }
    }
    return 0;
}

/// Callback: Receive data chunk
static int on_data_chunk_recv_callback(nghttp2_session* session, uint8_t flags, int32_t stream_id, const uint8_t* data, size_t len, void* user_data)
{
    Http2SessionData* session_data = static_cast<Http2SessionData*>(user_data);
    session_data->response.body.append(reinterpret_cast<const char*>(data), len);
    return 0;
}

/// Callback: Stream closed
static int on_stream_close_callback(nghttp2_session* session, int32_t stream_id, uint32_t error_code, void* user_data)
{
    Http2SessionData* session_data = static_cast<Http2SessionData*>(user_data);
    session_data->response_complete = true;
    return 0;
}

/// Callback: Frame received
static int on_frame_recv_callback(nghttp2_session* session, const nghttp2_frame* frame, void* user_data)
{
    if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
        Log::Debug("Response headers received");
    }
    return 0;
}

/// Make HTTP/2 request using nghttp2
HttpResponse makeHttp2Request(const std::string& host, int port, const std::string& path)
{
    HttpResponse response;
    Http2SessionData session_data;

    // Create socket
    session_data.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (session_data.sockfd == INVALID_SOCKET) {
        Log::Error("Failed to create socket");
        return response;
    }

    // Resolve hostname
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        Log::Error("Failed to resolve hostname: {}", host);
        close_socket(session_data.sockfd);
        return response;
    }

    // Connect to server
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(session_data.sockfd, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        Log::Error("Failed to connect to {}:{}", host, port);
        close_socket(session_data.sockfd);
        return response;
    }

    Log::Info("Connected to {}:{}", host, port);

    // Setup SSL/TLS with ALPN for HTTP/2
    SSL_library_init();
    SSL_load_error_strings();

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        Log::Error("Failed to create SSL context");
        close_socket(session_data.sockfd);
        return response;
    }

    // Set ALPN protocols (h2 for HTTP/2)
    const unsigned char alpn_proto[] = "\x02h2";
    SSL_CTX_set_alpn_protos(ctx, alpn_proto, sizeof(alpn_proto) - 1);

    session_data.ssl = SSL_new(ctx);
    SSL_set_fd(session_data.ssl, session_data.sockfd);

    if (SSL_connect(session_data.ssl) <= 0) {
        Log::Error("Failed to establish SSL connection");
        SSL_free(session_data.ssl);
        SSL_CTX_free(ctx);
        close_socket(session_data.sockfd);
        return response;
    }

    // Verify ALPN negotiation
    const unsigned char* alpn = nullptr;
    unsigned int alpnlen = 0;
    SSL_get0_alpn_selected(session_data.ssl, &alpn, &alpnlen);

    if (alpn == nullptr || alpnlen != 2 || memcmp("h2", alpn, 2) != 0) {
        Log::Error("HTTP/2 not negotiated via ALPN");
        SSL_shutdown(session_data.ssl);
        SSL_free(session_data.ssl);
        SSL_CTX_free(ctx);
        close_socket(session_data.sockfd);
        return response;
    }

    Log::Info("HTTP/2 negotiated via ALPN");

    // Initialize nghttp2 session
    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);
    nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);
    nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk_recv_callback);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);

    nghttp2_session_client_new(&session_data.session, callbacks, &session_data);
    nghttp2_session_callbacks_del(callbacks);

    // Send client connection preface and SETTINGS frame
    nghttp2_settings_entry iv[] = {{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}};
    nghttp2_submit_settings(session_data.session, NGHTTP2_FLAG_NONE, iv, 1);

    // Submit HTTP/2 request
    nghttp2_nv hdrs[] = {
        {(uint8_t*)":method", (uint8_t*)"GET", 7, 3, NGHTTP2_NV_FLAG_NONE},
        {(uint8_t*)":scheme", (uint8_t*)"https", 7, 5, NGHTTP2_NV_FLAG_NONE},
        {(uint8_t*)":authority", (uint8_t*)host.c_str(), 10, host.size(), NGHTTP2_NV_FLAG_NONE},
        {(uint8_t*)":path", (uint8_t*)path.c_str(), 5, path.size(), NGHTTP2_NV_FLAG_NONE},
        {(uint8_t*)"user-agent", (uint8_t*)"nghttp2-client", 10, 14, NGHTTP2_NV_FLAG_NONE}
    };

    int32_t stream_id = nghttp2_submit_request(session_data.session, nullptr, hdrs, 5, nullptr, nullptr);
    if (stream_id < 0) {
        Log::Error("Failed to submit HTTP/2 request");
        nghttp2_session_del(session_data.session);
        SSL_shutdown(session_data.ssl);
        SSL_free(session_data.ssl);
        SSL_CTX_free(ctx);
        close_socket(session_data.sockfd);
        return response;
    }

    Log::Info("HTTP/2 request submitted (stream ID: {})", stream_id);

    // Send queued frames
    nghttp2_session_send(session_data.session);

    // Receive and process responses
    char buffer[4096];
    while (!session_data.response_complete) {
        int nread = SSL_read(session_data.ssl, buffer, sizeof(buffer));
        Log::Debug("SSL_read result: {}", nread);
        if (nread <= 0) {
            break;
        }

        ssize_t readlen = nghttp2_session_mem_recv(session_data.session, reinterpret_cast<uint8_t*>(buffer), nread);
        Log::Debug("nghttp2_session_mem_recv result: {}", readlen);
        if (readlen < 0) {
            Log::Error("nghttp2_session_mem_recv failed: {}", nghttp2_strerror(readlen));
            break;
        }

        // Send any pending frames
        nghttp2_session_send(session_data.session);
    }

    response = session_data.response;

    // Cleanup
    nghttp2_session_del(session_data.session);
    SSL_shutdown(session_data.ssl);
    SSL_free(session_data.ssl);
    SSL_CTX_free(ctx);
    close_socket(session_data.sockfd);

    return response;
}
