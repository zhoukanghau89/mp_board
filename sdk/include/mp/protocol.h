#pragma once
#include <cstdint>
#include <cstring>

#include "mp/crc.h"

namespace mp {

// 板间串口帧：AA55 | mod_id | length | payload | crc16(低字节在前)
constexpr uint16_t kFrameHead = 0xAA55;
constexpr size_t   kFrameOverhead = 2 + 1 + 1 + 2;

enum ModId : uint8_t {
    kModUnknown  = 0x00,
    kModWheel    = 0x01,   // 底盘轮速
    kModBattery  = 0x02,   // 电池
    kModImu      = 0x03,
    kModCutter   = 0x04,   // 刀盘
};

#pragma pack(push, 1)
struct WheelPayload {
    float   left_rpm;
    float   right_rpm;
    float   voltage;
};

struct BatteryPayload {
    uint16_t voltage_mv;
    int16_t  current_ma;
    uint8_t  soc;          // 0-100
    uint8_t  status;       // 0 空闲 1 充电 2 放电 3 故障
};

struct CutterPayload {
    uint16_t rpm;
    uint8_t  on;
    uint8_t  err;
};
#pragma pack(pop)

// 共享内存里的一份整机快照
struct ModSnapshot {
    uint32_t stamp_ms;
    float    left_rpm;
    float    right_rpm;
    float    voltage;
    uint8_t  soc;
    uint8_t  battery_status;
    uint16_t cutter_rpm;
    uint8_t  cutter_on;
    uint8_t  reserved[3];
};

// 下行指令
struct MotorCmd {
    int16_t left;
    int16_t right;
};

// 编码：返回整帧长度，缓冲不足返回 0
inline size_t EncodeFrame(uint8_t* out, size_t cap, uint8_t mod_id,
                          const void* payload, uint8_t len) {
    const size_t total = kFrameOverhead + len;
    if (cap < total) return 0;
    out[0] = static_cast<uint8_t>(kFrameHead & 0xFF);
    out[1] = static_cast<uint8_t>((kFrameHead >> 8) & 0xFF);
    out[2] = mod_id;
    out[3] = len;
    if (len > 0 && payload != nullptr) std::memcpy(out + 4, payload, len);
    const uint16_t crc = Crc16(out, 4 + len);
    out[4 + len] = static_cast<uint8_t>(crc & 0xFF);
    out[5 + len] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    return total;
}

// 解码：成功返回载荷长度，失败返回 -1
inline int DecodeFrame(const uint8_t* in, size_t len, uint8_t* mod_id,
                       uint8_t* payload, size_t payload_cap) {
    if (len < kFrameOverhead) return -1;
    if (in[0] != (kFrameHead & 0xFF) || in[1] != ((kFrameHead >> 8) & 0xFF)) return -1;
    const uint8_t id = in[2];
    const uint8_t plen = in[3];
    if (len < kFrameOverhead + plen) return -1;
    const uint16_t expect = Crc16(in, 4 + plen);
    const uint16_t got = static_cast<uint16_t>(in[4 + plen]) |
                                              static_cast<uint16_t>(in[5 + plen] << 8);
    if (expect != got) return -1;
    if (plen > payload_cap) return -1;
    if (plen > 0) std::memcpy(payload, in + 4, plen);
    *mod_id = id;
    return plen;
}

}  // namespace mp
