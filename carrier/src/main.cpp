// 板间通信进程：串口收帧 -> 更新共享内存 -> 发布事件
#include <chrono>
#include <csignal>
#include <cstring>
#include <string>
#include <thread>

#include "mp/bus.h"
#include "mp/config.h"
#include "mp/log.h"
#include "mp/protocol.h"
#include "mp/serial.h"
#include "mp/shm.h"

namespace {
volatile sig_atomic_t g_running = 1;
void OnSignal(int) { g_running = 0; }

// 把一帧载荷合入快照
void ApplyFrame(uint8_t mod_id, const uint8_t* payload, int plen,
                mp::ModSnapshot* snap) {
  switch (mod_id) {
    case mp::kModWheel: {
      if (plen != static_cast<int>(sizeof(mp::WheelPayload))) return;
      mp::WheelPayload w{};
      std::memcpy(&w, payload, sizeof(w));
      snap->left_rpm  = w.left_rpm;
      snap->right_rpm = w.right_rpm;
      snap->voltage   = w.voltage;
      break;
    }
    case mp::kModBattery: {
      if (plen != static_cast<int>(sizeof(mp::BatteryPayload))) return;
      mp::BatteryPayload b{};
      std::memcpy(&b, payload, sizeof(b));
      snap->voltage        = b.voltage_mv / 1000.0f;
      snap->soc            = b.soc;
      snap->battery_status = b.status;
      break;
    }
    case mp::kModCutter: {
      if (plen != static_cast<int>(sizeof(mp::CutterPayload))) return;
      mp::CutterPayload c{};
      std::memcpy(&c, payload, sizeof(c));
      snap->cutter_rpm = c.rpm;
      snap->cutter_on  = c.on;
      break;
    }
    default:
      break;
  }
}
}  // namespace

int main(int argc, char** argv) {
  const std::string cfg_path = argc > 1 ? argv[1] : "config/default.conf";
  mp::Config cfg;
  cfg.Load(cfg_path);

  mp::LogConfig lc;
  lc.dir    = cfg.GetStr("common", "log_dir", "/tmp/mp_board/logs");
  lc.prefix = "carrier";
  lc.level  = static_cast<mp::LogLevel>(cfg.GetInt("common", "log_level", 1));
  mp::Logger::Instance().Init(lc, "carrier");

  const int pub_port = cfg.GetInt("bus", "pub_port", 27788);
  const int sub_port = cfg.GetInt("bus", "sub_port", 27789);

  mp::Publisher pub;
  mp::Subscriber sub;
  if (!pub.Connect(mp::MakeEndpoint(sub_port))) {
    MP_LOG_ERROR("connect pub endpoint failed");
    return 1;
  }
  if (!sub.Connect(mp::MakeEndpoint(pub_port)) || !sub.Subscribe("")) {
    MP_LOG_ERROR("connect sub endpoint failed");
    return 1;
  }

  mp::SharedRegion<mp::ModSnapshot> shm;
  if (!shm.Open("/mp_board_mod", true)) {
    MP_LOG_ERROR("open shm failed");
    return 1;
  }
  MP_LOG_INFO("shm ready");

  const std::string dev  = cfg.GetStr("serial", "port", "/dev/ttyAS5");
  const int         baud = cfg.GetInt("serial", "baud", 115200);
  const bool        sim  = cfg.GetBool("serial", "sim", true);

  mp::SerialPort port;
  bool serial_ok = false;
  if (!sim) {
    serial_ok = port.Open(dev, baud);
    if (!serial_ok) MP_LOG_WARN("open %s failed, fallback to simulator", dev.c_str());
  } else {
    MP_LOG_INFO("simulation mode, no hardware serial");
  }

  mp::SerialSimulator sim_wheel(mp::kModWheel);
  mp::SerialSimulator sim_bat(mp::kModBattery);

  mp::ModSnapshot snap{};
  uint32_t seq = 0;
  auto last_pub = std::chrono::steady_clock::now();
  auto last_log = last_pub;

  std::signal(SIGINT, OnSignal);
  std::signal(SIGTERM, OnSignal);

  uint8_t buf[256];
  while (g_running) {
    int n = 0;
    if (serial_ok) {
      n = port.Read(buf, sizeof(buf), 20);
    } else {
      // 多模块混合上行：3 帧轮速 + 1 帧电池
      static uint32_t frame_no = 0;
      ++frame_no;
      if ((frame_no % 4) == 0) {
        n = sim_bat.Fill(buf, sizeof(buf));
      } else {
        n = sim_wheel.Fill(buf, sizeof(buf));
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (n < 0) {
      MP_LOG_ERROR("serial read error");
      break;
    }

    if (n > 0) {
      uint8_t mod_id = 0;
      uint8_t payload[64];
      const int plen = mp::DecodeFrame(buf, static_cast<size_t>(n), &mod_id,
                                       payload, sizeof(payload));
      if (plen < 0) {
        MP_LOG_WARN("decode frame failed, %d bytes", n);
      } else {
        snap.stamp_ms = static_cast<uint32_t>(mp::NowMs());
        ApplyFrame(mod_id, payload, plen, &snap);
        seq = shm.Store(snap);
        if ((seq % 4) == 0) {
          mp::Event ev = mp::Event::MakeRaw(mp::kEvModUpdate, &snap, sizeof(snap));
          ev.seq = seq;
          pub.Publish(ev);
        }
      }
    }

    // 处理下行电机指令
    mp::Event ev;
    while (sub.Poll(&ev, 0)) {
      if (ev.id == mp::kEvMotorCmd) {
        mp::MotorCmd cmd{};
        if (ev.As(&cmd)) {
          MP_LOG_INFO("motor cmd left=%d right=%d", cmd.left, cmd.right);
        }
      }
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - last_log > std::chrono::seconds(10)) {
      last_log = now;
      MP_LOG_INFO("alive rpm=%.1f/%.1f soc=%u seq=%u", snap.left_rpm,
                  snap.right_rpm, snap.soc, seq);
    }
    (void)last_pub;
  }

  MP_LOG_INFO("carrier exit");
  shm.Close();
  shm.Unlink();
  mp::Logger::Instance().Shutdown();
  return 0;
}
