#include <http/response.hpp>
#include <http/socket.hpp>
#include <sstream>

void http::send_response(http::socket& sock, http::response r) {
    std::ostringstream sstr;
    sstr << "HTTP/1.1 " << r.code << "[ReasonHere]\r\n";
    sstr << "\r\n";
    sstr << r.body;
    std::string bytes = sstr.str();
    sock.send_all(
        reinterpret_cast<const unsigned char*>(bytes.data()),
        static_cast<ssize_t>(bytes.size())
    );
}