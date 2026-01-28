#pragma once

/// Cross-platform socket compatibility layer
/// Provides unified interface for POSIX and Windows sockets

#ifdef _WIN32
    // Windows-specific
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <BaseTsd.h>  // For SSIZE_T
    
    // Map POSIX types to Windows equivalents
    // Note: ssize_t is used by nghttp2, so we need to define it before including nghttp2.h
    #ifndef ssize_t
    using ssize_t = SSIZE_T;
    #endif
    
    using socklen_t = int;
    
    // Map POSIX functions to Windows equivalents
    inline int close_socket(SOCKET s) { return closesocket(s); }
    
    // Error handling
    inline int get_socket_error() { return WSAGetLastError(); }
    
    // Initialize Winsock (call once at startup)
    inline bool init_sockets() {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }
    
    // Cleanup Winsock (call once at shutdown)
    inline void cleanup_sockets() {
        WSACleanup();
    }
    
#else
    // Unix/POSIX
    #include <arpa/inet.h>
    #include <cerrno>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    
    // POSIX already has ssize_t
    using SOCKET = int;
    constexpr SOCKET INVALID_SOCKET = -1;
    constexpr int SOCKET_ERROR = -1;
    
    // Map Windows functions to POSIX equivalents
    inline int close_socket(int s) { return close(s); }
    
    // Error handling
    inline int get_socket_error() { return errno; }
    
    // No initialization needed for POSIX sockets
    inline bool init_sockets() { return true; }
    inline void cleanup_sockets() {}
    
#endif
