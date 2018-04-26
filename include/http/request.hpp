#ifndef COMP4621_REQUEST_HPP_INCLUDED
#define COMP4621_REQUEST_HPP_INCLUDED
#include <string>
#include <unordered_map>
namespace http {
    struct socket;
    struct request {
        std::string method;
        std::string uri;
        std::string version;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };
    request recv_request(socket& sock);
}
#endif