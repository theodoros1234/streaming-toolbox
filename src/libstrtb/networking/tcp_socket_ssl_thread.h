#ifndef STRTB_NETWORKING_TCP_SOCKET_SSL_THREAD_H
#define STRTB_NETWORKING_TCP_SOCKET_SSL_THREAD_H

#include <thread>
#include <openssl/ssl.h>
#include <mutex>
#include <condition_variable>

namespace strtb::networking {

// helper thread for safe simultaneous send/recv on an SSL/TLS-encrypted socket
class tcp_socket_ssl_thread {
private:
    std::mutex _lock;
    std::condition_variable _cv_read, _cv_write;
    std::thread _t;
    bool _thread_active = false;
    int _sock = -1, _eventfd = -1;
    SSL* _ssl = nullptr;
    char* _buffer_read = nullptr;
    const char* _buffer_write = nullptr;
    size_t _length_read, _length_write;
    bool _successful_read, _successful_write, _available;
    bool _requested_read, _requested_write, _requested_shutdown,
         _requested_available, _requested_close, _shutdown_sent;
    int _errno_ssl, _errno_syscall;
    void _decide_exception();

protected:
    void thread_loop();
    friend std::thread;

public:
    tcp_socket_ssl_thread();
    ~tcp_socket_ssl_thread();
    tcp_socket_ssl_thread(const tcp_socket_ssl_thread&) = delete;
    tcp_socket_ssl_thread(tcp_socket_ssl_thread&&) = delete;
    void start(int sock, SSL* ssl);
    void stop();
    size_t recv(char* buffer, size_t length);
    void send(const char* buffer, size_t length);
    void shutdown_gracefully();
    bool available();
    void release();
};

}

#endif // STRTB_NETWORKING_TCP_SOCKET_SSL_THREAD_H
