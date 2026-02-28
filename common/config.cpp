#include "common/config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace aie {

Config::Config()
{
}

Config::~Config()
{
}

int Config::load(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open())
        return -1;

    std::string line;
    while (std::getline(f, line)) {
        /* remove comments */
        auto pos = line.find('#');
        if (pos != std::string::npos) {
            line.erase(pos);
        }
        line = trim(line);
        if (line.empty())
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (!key.empty()) {
            data_[key] = val;
        }
    }

    return 0;
}

std::string Config::get(const std::string &key, const std::string &def) const
{
    auto it = data_.find(key);
    if (it != data_.end())
        return it->second;
    return def;
}

bool Config::has(const std::string &key) const
{
    return data_.find(key) != data_.end();
}

std::string Config::trim(const std::string &s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // namespace aie
