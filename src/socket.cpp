#include <http/socket.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <boost/log/trivial.hpp>
void errmaybe(int);

http::socket::socket(int connected) : sockfd(connected), buffer{0}, data_begin(begin(buffer)), data_end(data_begin) {
};

std::optional<std::string> http::socket::recvline() {
    std::ostringstream line;
    auto line_end = std::search(data_begin, data_end, begin(crlf), end(crlf));
    // While we have not encountered a crlf in our buffer
    while(line_end == data_end) {
        // Add what we have so far to our line
        std::copy(data_begin, data_end, std::ostream_iterator<char>(line));
        // Read more data
        data_begin = begin(buffer);
        int n = ::recv(sockfd, data_begin, buffer_size, 0);
        errmaybe(n);
        if(n == 0) {
            return std::nullopt;
        }
        data_end = data_begin + n;
        // Search for newline again
        line_end = std::search(data_begin, data_end, begin(crlf), end(crlf));
    }
    //We've found a line, copy it
    std::copy(data_begin, line_end, std::ostream_iterator<char>(line));
    data_begin = line_end + crlf.size();
    return line.str();
}

void http::socket::send_all(const unsigned char* start, ssize_t size) {
    while(size > 0) {
        ssize_t sent = ::send(sockfd, start, size, 0);
        errmaybe(sent);
        size -= sent;
    }
}

http::socket::~socket() {
    ::close(sockfd);
}