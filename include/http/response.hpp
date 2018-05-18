#ifndef COMP4621_RESPONSE_HPP_INCLUDED
#define COMP4621_RESPONSE_HPP_INCLUDED
#include <string>
#include <unordered_map>
namespace http {
    struct response {
        int code;
        std::string reason;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };
}
#endif