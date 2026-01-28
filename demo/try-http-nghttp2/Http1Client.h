#pragma once

#include <string>

/// HTTP response structure
struct HttpResponse
{
    int status_code = 0;
    std::string headers;
    std::string body;
};

/// Make HTTP or HTTPS request to a server
HttpResponse makeHttpRequest(const std::string& host, int port, const std::string& path, bool use_tls = false);
