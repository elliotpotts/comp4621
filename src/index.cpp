#include <http/index.hpp>
#include <iterator>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <fmt/format.h>
#include <tuple>

namespace fs = boost::filesystem;

std::string get_content_type(fs::path path) {
    auto e = path.extension();
    if      (e == ".css")   return "text/css";
    else if (e == ".html")  return "text/html";
    else if (e == ".eot")   return "application/vnd.ms-fontobject";
    else if (e == ".svg")   return "image/svg+xml";
    else if (e == ".ttf")   return "font/ttf";
    else if (e == ".woff")  return "font/woff";
    else if (e == ".woff2") return "font/woff2";
    else if (e == ".webm")  return "video/webm";
    else return "text/plain";
}

std::string get_icon(fs::path path) {
    auto e = path.extension();
    if      (e == ".css" || e == ".html" || e == ".json" || e == ".cpp") return "icon-file-code";
    else if (e == ".webm")  return "icon-file-video";
    else if (e == ".pdf")   return "icon-file-pdf";
    else if (e == ".png" || e == ".jpg") return "icon-file-image";
    else if (e == ".txt") return "icon-doc-text";
    else return "icon-doc";
}

http::response http::serve_file(fs::path p, std::fstream& stream) {
    return {200, "OK", get_content_type(p), {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}
    }};
}

void write_file(std::string path, std::stringstream& to) {
    std::ifstream file{path};
    std::copy(
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{},
        std::ostreambuf_iterator<char>{to}
    );
}

static const std::string index_template = R"EOS(
<!DOCTYPE html>
<html>
    <head>
        <title>Index of {0}</title>
        <link href="data:image/x-icon;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQEAYAAABPYyMiAAAABmJLR0T///////8JWPfcAAAACXBIWXMAAABIAAAASABGyWs+AAAAF0lEQVRIx2NgGAWjYBSMglEwCkbBSAcACBAAAeaR9cIAAAAASUVORK5CYII=" rel="icon" type="image/x-icon" />
        <style>
        html {{ font-family: monospace; sans-serif; }}
        table {{ min-width: 800px; }}
        td, td {{ text-align: center; }}
        td:nth-of-type(1) {{ text-align: left; padding-left: 2em; }}
        </style>
    </head>
    <body>
        <h1>Index of {0}</h1>
        <table>
            <thead>
                <th>Name</th>
                <th>Last Modified</th>
                <th>Size</th>
                <th>Type</th>
            </thead>
            <tbody>
            {1}
            </tbody>
        </table>
    </body>
</html>
)EOS";

std::string last_modified_string(fs::path path) {
    std::ostringstream sstream;
    const std::time_t last_modified_raw = fs::last_write_time(path);
    const std::tm* last_modified = std::localtime(&last_modified_raw);
    sstream << std::put_time(last_modified, "%c, %Z");
    return sstream.str();
}

std::string format_row(fs::path path) {
    bool is_dir = fs::is_directory(path);
    static const std::string row_template =
    "<tr><td><a href='{}{}'>{}</a></td><td>{}</td><td>{}</td><td>{}</td></tr>";
    return fmt::format(row_template,
        path.filename().string(),
        is_dir ? "/" : "",
        path.filename().string(),
        is_dir ? "" : last_modified_string(path),
        is_dir ? "" : std::to_string(fs::file_size(path)),
        is_dir ? "Directory" : "Document"
    );
}

http::response http::serve_index(fs::path requested_path, fs::path mapped_path) {
    std::stringstream rows;
    std::vector<fs::directory_entry> entries;
    std::copy(
        fs::directory_iterator{mapped_path},
        fs::directory_iterator{},
        std::back_inserter(entries)
    );
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs){
        return std::make_tuple(!fs::is_directory(lhs), lhs.path().filename().string())
             < std::make_tuple(!fs::is_directory(rhs), rhs.path().filename().string());
    });
    for(const auto& entry : entries) {
        rows << format_row(entry);
    }
    return {
        200, "OK", "text/html; charset=utf-8",
        fmt::format(index_template, requested_path.string(), rows.str())
    };
}

static const std::string error_404_template = R"EOS(
<!DOCTYPE html>
<html>
    <head>
        <title>404</title>
        <link href="data:image/x-icon;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQEAYAAABPYyMiAAAABmJLR0T///////8JWPfcAAAACXBIWXMAAABIAAAASABGyWs+AAAAF0lEQVRIx2NgGAWjYBSMglEwCkbBSAcACBAAAeaR9cIAAAAASUVORK5CYII=" rel="icon" type="image/x-icon" />
        <style>
        html {{ font-family: monospace; sans-serif; }}
        </style>
    </head>
    <body>
        <h1>Could not find the file specified: {0}</h1>
    </body>
</html>
)EOS";

http::response http::serve_404(fs::path requested_path) {
    return {
        404, "File Not Found", "text/html; charset=utf-8",
        fmt::format(error_404_template, requested_path.string())
    };
}