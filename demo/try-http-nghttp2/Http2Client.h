#pragma once

#include "Http1Client.h"
#include <string>

/// Make HTTP/2 request using nghttp2
HttpResponse makeHttp2Request(const std::string& host, int port, const std::string& path);
