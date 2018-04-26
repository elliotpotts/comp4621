#ifndef COMP4621_RESPONSE_HPP_INCLUDED
#define COMP4621_RESPONSE_HPP_INCLUDED
namespace http {
    struct response {
        int code;
        static response bad_request() { return {400}; }
        static response version_not_supported() { return {505}; }
    };
}
#endif