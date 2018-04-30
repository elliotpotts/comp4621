#ifndef COMP4621_TCP_SOCKET_HPP_INCLUDED
#define COMP4621_TCP_SOCKET_HPP_INCLUDED
#include <array>
#include <optional>
#include <sys/types.h>
namespace http {
    const std::string crlf = "\r\n";

    // Line-buffered tcp socket
    class socket {
        static const int buffer_size = 2048; // arbitraty
        using byte_buf = std::array<char, buffer_size>;
        int sockfd;
        byte_buf buffer;
        byte_buf::iterator data_begin;
        byte_buf::iterator data_end;

        public:
        socket(int connected);
        std::optional<std::string> recvline();
        void send_all(const unsigned char* start, ssize_t size);
        ~socket();
    };
}
#endif