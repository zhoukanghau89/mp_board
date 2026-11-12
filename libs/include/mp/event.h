#pragma once
#include <cstdint>
#include <cstring>
#include <time.h>
#include <vector>

namespace mp {

// 事件 ID：高 4 位域，低 12 位类型。新增时只追加，不改已有值。
enum EventId : uint16_t {
    kEvNone        = 0x0000,
    kEvHeartbeat   = 0x0001,

    // 传感器域 0x1xxx
    kEvModUpdate   = 0x1001,  // 共享内存快照已更新
    kEvMotorCmd    = 0x1002,

    // 任务域 0x2xxx
    kEvTaskCmd     = 0x2001,
    kEvTaskState   = 0x2002,

    // 网络域 0x3xxx
    kEvNetStatus   = 0x3001,
    kEvOtaProgress = 0x3002,

    // 系统域 0x4xxx
    kEvLog         = 0x4001,
    kEvFault       = 0x4002,
};

#pragma pack(push, 1)
struct EventHeader {
    uint16_t id;
    uint16_t version;
    uint32_t seq;
    uint64_t ts_ms;
    uint32_t len;
};
#pragma pack(pop)

struct Event {
    uint16_t id = kEvNone;
    uint16_t version = 1;
    uint32_t seq = 0;
    uint64_t ts_ms = 0;
    std::vector<uint8_t> payload;

    template <typename T>
    bool As(T* out) const {
        if (out == nullptr || payload.size() < sizeof(T)) return false;
        std::memcpy(out, payload.data(), sizeof(T));
        return true;
    }

    template <typename T>
    static Event Make(uint16_t id, const T& value) {
        Event ev;
        ev.id = id;
        ev.version = 1;
        ev.payload.resize(sizeof(T));
        std::memcpy(ev.payload.data(), &value, sizeof(T));
        return ev;
    }

    static Event MakeRaw(uint16_t id, const void* data, uint32_t len);
};

inline uint64_t NowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000ull +
                  static_cast<uint64_t>(ts.tv_nsec / 1000000);
}

}  // namespace mp
