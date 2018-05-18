#ifndef COMP4621_SESSION_HPP_INCLUDED
#define COMP4621_SESSION_HPP_INCLUDED
#include <array>
#include <optional>
#include <string>
#include <http/request.hpp>
#include <http/response.hpp>
namespace http {
    class session {
        static const int buffer_size = 2048;
        using byte_buf = std::array<char, buffer_size>;

        int sockfd;
        byte_buf buffer;
        byte_buf::iterator data_begin;
        byte_buf::iterator data_end;

        http::request current_request;

        std::string recv_line();
        request recv_request();
        void send_all(const unsigned char* start, ssize_t size);
        void transfer_id(http::response);
        void transfer_chunked(http::response, std::size_t chunk_size = 12);
        http::response encode_id(http::response);
        http::response encode_gzip(http::response);

        protected:
        void send_response(http::response);
        void handle_request(http::request);

        public:
        void operator()(int sockfd);
    };
}
#endif