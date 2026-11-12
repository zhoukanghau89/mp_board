#include "mp/config.h"

#include <cstdlib>
#include <fstream>

namespace mp {

std::string Config::Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool Config::Load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;

    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            continue;
        }
        const size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, pos));
        const std::string val = Trim(line.substr(pos + 1));
        kv_[section + "." + key] = val;
    }
    return true;
}

std::string Config::GetStr(const std::string& section, const std::string& key,
                           const std::string& def) const {
    const auto it = kv_.find(section + "." + key);
    return it == kv_.end() ? def : it->second;
}

int Config::GetInt(const std::string& section, const std::string& key, int def) const {
    const auto it = kv_.find(section + "." + key);
    if (it == kv_.end()) return def;
    char* end = nullptr;
    const long v = std::strtol(it->second.c_str(), &end, 10);
    if (end == it->second.c_str()) return def;
    return static_cast<int>(v);
}

bool Config::GetBool(const std::string& section, const std::string& key, bool def) const {
    const auto it = kv_.find(section + "." + key);
    if (it == kv_.end()) return def;
    const std::string& v = it->second;
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

}  // namespace mp
