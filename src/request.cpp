#include <http/request.hpp>
#include <http/response.hpp>
#include <http/socket.hpp>
#include <algorithm>
#include <string>

http::request http::recv_request(http::socket& sock) {
    std::string reqln = sock.recvline();
    auto reqln_end = end(reqln);

    auto method_start = begin(reqln);
    auto method_end = std::find(method_start, reqln_end, ' ');
    if(method_end == reqln_end) throw http::response::bad_request();

    auto req_uri_start = method_end + 1;
    auto req_uri_end = std::find(req_uri_start, reqln_end, ' ');
    if(req_uri_end == reqln_end) throw http::response::bad_request();

    auto version_start = req_uri_end + 1;
    auto version_end = reqln_end;

    // headers
    //for(std::string line = sock.recvline(); !line.empty();) {
        //std::string headerln = sock.recvln();
    //}

    return {
        {method_start, method_end},
        {req_uri_start, req_uri_end},
        {version_start, version_end}
    };
}