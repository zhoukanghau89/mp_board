#pragma once
#include <map>
#include <string>

namespace mp {

// 极简 ini 解析：支持 [section] 与 key = value，# 开头为注释
class Config {
  public:
    bool Load(const std::string& path);

    std::string GetStr(const std::string& section, const std::string& key,
                       const std::string& def) const;
    int GetInt(const std::string& section, const std::string& key, int def) const;
    bool GetBool(const std::string& section, const std::string& key, bool def) const;

  private:
    static std::string Trim(const std::string& s);
    std::map<std::string, std::string> kv_;  // "section.key" -> value
};

}  // namespace mp
