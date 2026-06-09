#include "cheat/cheat_parser.hpp"

#include <fstream>
#include <sstream>

namespace czgba::cheat {
namespace {

std::string trim(std::string value)
{
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    std::size_t first = 0;
    while (first < value.size() && (value[first] == ' ' || value[first] == '\t')) {
        ++first;
    }
    return value.substr(first);
}

std::string make_id(const std::string& name, std::size_t index)
{
    std::string id;
    for (const char ch : name) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            id.push_back(ch);
        } else if (ch >= 'A' && ch <= 'Z') {
            id.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else if (!id.empty() && id.back() != '-') {
            id.push_back('-');
        }
    }
    if (id.empty()) {
        id = "cheat";
    }
    return id + "-" + std::to_string(index + 1);
}

} // namespace

std::vector<Cheat> CheatParser::parse_file(const std::filesystem::path& path) const
{
    std::ifstream in(path);
    if (!in) {
        return {};
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    return parse_text(buffer.str());
}

std::vector<Cheat> CheatParser::parse_text(const std::string& text) const
{
    std::vector<Cheat> cheats;
    Cheat current;
    std::istringstream in(text);
    std::string line;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            if (!current.name.empty() || !current.codes.empty()) {
                current.id = make_id(current.name, cheats.size());
                cheats.push_back(std::move(current));
                current = {};
            }
            current.name = trim(line.substr(1, line.size() - 2));
        } else {
            if (current.name.empty()) {
                current.name = "Cheat";
            }
            current.codes.push_back(line);
        }
    }

    if (!current.name.empty() || !current.codes.empty()) {
        current.id = make_id(current.name, cheats.size());
        cheats.push_back(std::move(current));
    }

    return cheats;
}

} // namespace czgba::cheat
