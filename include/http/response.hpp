#ifndef COMP4621_RESPONSE_HPP_INCLUDED
#define COMP4621_RESPONSE_HPP_INCLUDED
#include <string>
namespace http {
    struct response {
        int code;
        std::string reason;
        std::string content_type;
        std::string body;
    };
}
#endif