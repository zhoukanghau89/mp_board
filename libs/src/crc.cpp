#include "mp/crc.h"

namespace mp {

// CRC16/MODBUS：多项式 0xA001，初值 0xFFFF
uint16_t Crc16(const void* data, size_t len) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1u) {
                crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001);
            } else {
                crc = static_cast<uint16_t>(crc >> 1);
            }
        }
    }
    return crc;
}

}  // namespace mp
