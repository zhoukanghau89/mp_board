#include "mp/bus.h"

#include <zmq.h>

#include <cstdio>
#include <cstring>

namespace mp {
namespace {

// 每个进程一个 ZMQ 上下文，所有 socket 共享
void* Ctx() {
    static void* ctx = zmq_ctx_new();
    return ctx;
}

constexpr int kSendTimeoutMs = 300;

}  // namespace

Event Event::MakeRaw(uint16_t id, const void* data, uint32_t len) {
    Event ev;
    ev.id = id;
    ev.version = 1;
    ev.ts_ms = NowMs();
    ev.payload.resize(len);
    if (len > 0 && data != nullptr) std::memcpy(ev.payload.data(), data, len);
    return ev;
}

std::string MakeEndpoint(int port) {
    return "tcp://127.0.0.1:" + std::to_string(port);
}

Publisher::~Publisher() { Close(); }

bool Publisher::Connect(const std::string& endpoint) {
    Close();
    sock_ = zmq_socket(Ctx(), ZMQ_PUB);
    if (sock_ == nullptr) return false;

    int linger = 0;
    zmq_setsockopt(sock_, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_setsockopt(sock_, ZMQ_SNDTIMEO, &kSendTimeoutMs, sizeof(kSendTimeoutMs));

    if (zmq_connect(sock_, endpoint.c_str()) != 0) {
        zmq_close(sock_);
        sock_ = nullptr;
        return false;
    }
    return true;
}

void Publisher::Close() {
    if (sock_ != nullptr) {
        zmq_close(sock_);
        sock_ = nullptr;
    }
}

bool Publisher::Publish(const Event& ev) {
    if (sock_ == nullptr) return false;

    EventHeader hdr{};
    hdr.id      = ev.id;
    hdr.version = ev.version;
    hdr.seq     = ev.seq;
    hdr.ts_ms   = ev.ts_ms == 0 ? NowMs() : ev.ts_ms;
    hdr.len     = static_cast<uint32_t>(ev.payload.size());

    const size_t total = sizeof(hdr) + ev.payload.size();
    // 小于 1KB 的事件走栈上缓冲，避免每条消息一次堆分配
    unsigned char stack_buf[1024];
    unsigned char* buf = total <= sizeof(stack_buf) ? stack_buf : new unsigned char[total];

    std::memcpy(buf, &hdr, sizeof(hdr));
    if (!ev.payload.empty()) {
        std::memcpy(buf + sizeof(hdr), ev.payload.data(), ev.payload.size());
    }

    const int rc = zmq_send(sock_, buf, total, ZMQ_DONTWAIT);
    if (buf != stack_buf) delete[] buf;
    return rc == static_cast<int>(total);
}

Subscriber::~Subscriber() { Close(); }

bool Subscriber::Connect(const std::string& endpoint) {
    Close();
    sock_ = zmq_socket(Ctx(), ZMQ_SUB);
    if (sock_ == nullptr) return false;

    int linger = 0;
    zmq_setsockopt(sock_, ZMQ_LINGER, &linger, sizeof(linger));
    if (zmq_connect(sock_, endpoint.c_str()) != 0) {
        zmq_close(sock_);
        sock_ = nullptr;
        return false;
    }
    return true;
}

bool Subscriber::Subscribe(const std::string& topic) {
    if (sock_ == nullptr) return false;
    return zmq_setsockopt(sock_, ZMQ_SUBSCRIBE, topic.data(), topic.size()) == 0;
}

bool Subscriber::Poll(Event* out, int timeout_ms) {
    if (sock_ == nullptr || out == nullptr) return false;

    zmq_pollitem_t item{};
    item.socket = sock_;
    item.events = ZMQ_POLLIN;
    const int rc = zmq_poll(&item, 1, timeout_ms);
    if (rc <= 0 || (item.revents & ZMQ_POLLIN) == 0) return false;

    zmq_msg_t msg;
    zmq_msg_init(&msg);
    const int n = zmq_msg_recv(&msg, sock_, ZMQ_DONTWAIT);
    if (n < static_cast<int>(sizeof(EventHeader))) {
        zmq_msg_close(&msg);
        return false;
    }

    EventHeader hdr{};
    std::memcpy(&hdr, zmq_msg_data(&msg), sizeof(hdr));
    const auto* body = static_cast<const unsigned char*>(zmq_msg_data(&msg)) + sizeof(hdr);
    const size_t body_len = static_cast<size_t>(n) - sizeof(hdr);

    out->id      = hdr.id;
    out->version = hdr.version;
    out->seq     = hdr.seq;
    out->ts_ms   = hdr.ts_ms;
    out->payload.assign(body, body + body_len);

    zmq_msg_close(&msg);
    return true;
}

void Subscriber::Close() {
    if (sock_ != nullptr) {
        zmq_close(sock_);
        sock_ = nullptr;
    }
}

}  // namespace mp
