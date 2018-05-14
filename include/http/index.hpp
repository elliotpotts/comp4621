#ifndef COMP4621_INDEX_HPP_INCLUDED
#define COMP4621_INDEX_HPP_INCLUDED
#include <http/response.hpp>
#include <boost/filesystem.hpp>
namespace http {
    http::response serve_file(boost::filesystem::path p, std::fstream& stream);
    http::response serve_index(boost::filesystem::path requested_path, boost::filesystem::path mapped_path);
    http::response serve_404(boost::filesystem::path);
}

#endif