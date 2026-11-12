#pragma once
#include <cstdarg>
#include <cstdio>
#include <string>

namespace mp {

enum class LogLevel { kDebug = 0, kInfo = 1, kWarn = 2, kError = 3 };

struct LogConfig {
    std::string dir       = "/tmp/mp_board/logs";
    std::string prefix    = "app";
    size_t      max_bytes = 10u * 1024u * 1024u;  // 单文件上限
    int         keep      = 5;                    // 保留份数
    bool        console   = true;
    LogLevel    level     = LogLevel::kInfo;
};

// 线程安全的文件日志：按大小轮转，保留 keep 份
class Logger {
  public:
    static Logger& Instance();

    void Init(const LogConfig& cfg, const std::string& module);
    void Write(LogLevel lv, const char* fmt, ...);
    void Shutdown();

    void set_level(LogLevel lv) { level_ = lv; }

  private:
    Logger() = default;
    ~Logger();
    void RotateIfNeeded(size_t incoming);
    static const char* LevelTag(LogLevel lv);

    std::string dir_;
    std::string prefix_;
    std::string module_;
    size_t max_bytes_ = 10u * 1024u * 1024u;
    int keep_ = 5;
    bool console_ = true;
    LogLevel level_ = LogLevel::kInfo;
    FILE* fp_ = nullptr;
    size_t written_ = 0;
    void* mutex_ = nullptr;  // std::mutex，避免头文件暴露
};

}  // namespace mp

#define MP_LOG_DEBUG(...) ::mp::Logger::Instance().Write(::mp::LogLevel::kDebug, __VA_ARGS__)
#define MP_LOG_INFO(...)  ::mp::Logger::Instance().Write(::mp::LogLevel::kInfo,  __VA_ARGS__)
#define MP_LOG_WARN(...)  ::mp::Logger::Instance().Write(::mp::LogLevel::kWarn,  __VA_ARGS__)
#define MP_LOG_ERROR(...) ::mp::Logger::Instance().Write(::mp::LogLevel::kError, __VA_ARGS__)
