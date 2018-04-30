#ifndef COMP4621_RESPONSE_HPP_INCLUDED
#define COMP4621_RESPONSE_HPP_INCLUDED
#include <string>
namespace http {
    struct socket;
    struct response {
        int code;
        std::string body;
    };
    void send_response(socket& sock, response r);
}
#endif