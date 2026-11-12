// 任务调度进程：事件驱动状态机
#include <chrono>
#include <csignal>
#include <cstdio>
#include <string>

#include "mp/bus.h"
#include "mp/config.h"
#include "mp/event.h"
#include "mp/log.h"
#include "mp/protocol.h"

namespace {
volatile sig_atomic_t g_running = 1;
void OnSignal(int) { g_running = 0; }

enum TaskState : uint8_t {
  kIdle = 0,
  kMowing = 1,
  kBackToCharger = 2,
  kCharging = 3,
  kFault = 4,
};

#pragma pack(push, 1)
struct TaskCmd {
  uint8_t action;  // 1 开始 2 回充 3 暂停
  uint8_t reserved[3];
};

struct TaskStateMsg {
  uint8_t  state;
  uint8_t  reserved;
  uint16_t progress;  // 0-100
};
#pragma pack(pop)

const char* StateName(uint8_t s) {
  switch (s) {
    case kIdle: return "IDLE";
    case kMowing: return "MOWING";
    case kBackToCharger: return "BACK_TO_CHARGER";
    case kCharging: return "CHARGING";
    case kFault: return "FAULT";
    default: return "UNKNOWN";
  }
}
}  // namespace

int main(int argc, char** argv) {
  const std::string cfg_path = argc > 1 ? argv[1] : "config/default.conf";
  mp::Config cfg;
  cfg.Load(cfg_path);

  mp::LogConfig lc;
  lc.dir    = cfg.GetStr("common", "log_dir", "/tmp/mp_board/logs");
  lc.prefix = "tasks";
  lc.level  = static_cast<mp::LogLevel>(cfg.GetInt("common", "log_level", 1));
  mp::Logger::Instance().Init(lc, "mptask");

  const int pub_port = cfg.GetInt("bus", "pub_port", 27788);
  const int sub_port = cfg.GetInt("bus", "sub_port", 27789);

  mp::Publisher pub;
  mp::Subscriber sub;
  if (!pub.Connect(mp::MakeEndpoint(sub_port)) || !sub.Connect(mp::MakeEndpoint(pub_port)) ||
      !sub.Subscribe("")) {
    MP_LOG_ERROR("bus connect failed");
    return 1;
  }

  std::signal(SIGINT, OnSignal);
  std::signal(SIGTERM, OnSignal);

  uint8_t state = kIdle;
  uint16_t progress = 0;
  uint8_t soc = 100;
  auto last_tick = std::chrono::steady_clock::now();
  auto last_pub = last_tick;

  auto SwitchTo = [&](uint8_t next) {
    if (state == next) return;
    MP_LOG_INFO("state %s -> %s", StateName(state), StateName(next));
    state = next;
    TaskStateMsg msg{state, 0, progress};
    pub.Publish(mp::Event::Make(mp::kEvTaskState, msg));
  };

  SwitchTo(kIdle);

  while (g_running) {
    mp::Event ev;
    while (sub.Poll(&ev, 20)) {
      if (ev.id == mp::kEvTaskCmd) {
        TaskCmd cmd{};
        if (!ev.As(&cmd)) continue;
        if (cmd.action == 1 && state == kIdle) {
          SwitchTo(kMowing);
        } else if (cmd.action == 2 && (state == kMowing || state == kIdle)) {
          SwitchTo(kBackToCharger);
        } else if (cmd.action == 3) {
          SwitchTo(kIdle);
        }
      } else if (ev.id == mp::kEvModUpdate) {
        mp::ModSnapshot snap{};
        if (ev.As(&snap)) soc = snap.soc;
      }
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - last_tick >= std::chrono::seconds(1)) {
      last_tick = now;
      switch (state) {
        case kMowing:
          if (soc > 0 && (progress % 2) == 0) --soc;
          if (progress < 100) ++progress;
          if (soc <= 20) SwitchTo(kBackToCharger);
          break;
        case kBackToCharger:
          if (progress > 0) --progress;
          if (progress == 0) SwitchTo(kCharging);
          break;
        case kCharging:
          if (soc < 100) ++soc;
          if (soc >= 95) SwitchTo(kIdle);
          break;
        default:
          if (soc < 100) ++soc;
          break;
      }
    }

    if (now - last_pub >= std::chrono::seconds(2)) {
      last_pub = now;
      TaskStateMsg msg{state, 0, progress};
      pub.Publish(mp::Event::Make(mp::kEvTaskState, msg));
    }
  }

  MP_LOG_INFO("tasks exit");
  mp::Logger::Instance().Shutdown();
  return 0;
}
