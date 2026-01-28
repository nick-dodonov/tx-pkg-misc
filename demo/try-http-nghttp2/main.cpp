#include "Boot/Boot.h"
#include "SocketCompat.h"  // Must be before nghttp2.h to define ssize_t on Windows
#include "Http1Client.h"
#include "Http2Client.h"
#include "Log/Log.h"
#include <iostream>
#include <nghttp2/nghttp2.h>
#include <set>
#include <string>

void runExample1()
{
    Log::Info("========== Request 1: HTTP ifconfig.io ==========");
    HttpResponse resp1 = makeHttpRequest("ifconfig.io", 80, "/", false);
    Log::Info("===== Status Code: {}", resp1.status_code);
    Log::Info("===== Headers:");
    std::cout << resp1.headers << "\n";
    Log::Info("===== Body:");
    std::cout << resp1.body << "\n";
}

void runExample2()
{
    Log::Info("========== Request 2: HTTPS httpbin.org /headers ==========");
    HttpResponse resp2 = makeHttpRequest("httpbin.org", 443, "/headers", true);
    Log::Info("===== Status Code: {}", resp2.status_code);
    Log::Info("===== Headers:");
    std::cout << resp2.headers << "\n";
    Log::Info("===== Body:");
    std::cout << resp2.body << "\n";
}

void runExample3()
{
    Log::Info("========== Request 3: HTTP/2 httpbin.org /get ==========");
    HttpResponse resp3 = makeHttp2Request("httpbin.org", 443, "/get");
    Log::Info("===== Status Code: {}", resp3.status_code);
    Log::Info("===== Headers:");
    std::cout << resp3.headers << "\n";
    Log::Info("===== Body:");
    std::cout << resp3.body << "\n";
}

int main(int argc, const char** argv)
{
    Boot::DefaultInit(argc, argv);

    // Initialize sockets (Winsock on Windows, no-op on Unix)
    if (!init_sockets()) {
        Log::Error("Failed to initialize sockets");
        return 1;
    }

    // Display nghttp2 version
    nghttp2_info* info = nghttp2_version(0);
    Log::Info("nghttp2 version: {}", info->version_str);

    // Parse command line arguments to determine which examples to run
    std::set<int> examples_to_run;

    if (argc > 1) {
        // Run specific examples based on arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "1") {
                examples_to_run.insert(1);
            } else if (arg == "2") {
                examples_to_run.insert(2);
            } else if (arg == "3") {
                examples_to_run.insert(3);
            } else {
                Log::Warn("Unknown argument: {}. Use 1, 2, or 3 to run specific examples.", arg);
            }
        }
    } else {
        // Default: run only example 3
        examples_to_run.insert(3);
    }

    // Run selected examples
    if (examples_to_run.contains(1)) {
        runExample1();
    }
    if (examples_to_run.contains(2)) {
        runExample2();
    }
    if (examples_to_run.contains(3)) {
        runExample3();
    }

    // Cleanup sockets (Winsock on Windows, no-op on Unix)
    cleanup_sockets();

    return 0;
}
