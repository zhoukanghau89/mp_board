#pragma once
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

namespace mp {

// 固定大小结构体 + seqlock 的共享内存区。
// 写方先写 seq++（奇数表示写入中），写完再 seq++，读方校验前后一致。
template <typename T>
class SharedRegion {
 public:
  SharedRegion() = default;
  ~SharedRegion() { Close(); }

  bool Open(const std::string& name, bool create) {
    name_ = name;
    const int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
    fd_ = ::shm_open(name.c_str(), flags, 0666);
    if (fd_ < 0) return false;
    if (create) {
      if (::ftruncate(fd_, static_cast<off_t>(sizeof(T))) != 0) {
        Close();
        return false;
      }
    }
    void* p = ::mmap(nullptr, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (p == MAP_FAILED) {
      Close();
      return false;
    }
    data_ = static_cast<Layout*>(p);
    if (create) {
      std::memset(data_, 0, sizeof(T));
      data_->seq.store(0);
    }
    return true;
  }

  // 写：返回写入后的序号
  uint32_t Store(const T& value) {
    if (data_ == nullptr) return 0;
    uint32_t s = data_->seq.load(std::memory_order_relaxed);
    data_->seq.store(s + 1, std::memory_order_release);  // 进入写
    data_->value = value;
    data_->seq.store(s + 2, std::memory_order_release);  // 写完
    return s + 2;
  }

  // 读：最多重试 3 次，返回 false 表示一直读写冲突
  bool Load(T* out) const {
    if (data_ == nullptr || out == nullptr) return false;
    for (int i = 0; i < 3; ++i) {
      const uint32_t s1 = data_->seq.load(std::memory_order_acquire);
      if (s1 & 1u) continue;
      *out = data_->value;
      const uint32_t s2 = data_->seq.load(std::memory_order_acquire);
      if (s1 == s2) return true;
    }
    return false;
  }

  bool valid() const { return data_ != nullptr; }

  void Close() {
    if (data_ != nullptr) {
      ::munmap(data_, sizeof(T));
      data_ = nullptr;
    }
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

  // 仅创建方调用，删除命名对象
  void Unlink() const {
    if (!name_.empty()) ::shm_unlink(name_.c_str());
  }

 private:
  struct Layout {
    std::atomic<uint32_t> seq;
    T value;
  };

  Layout* data_ = nullptr;
  int fd_ = -1;
  std::string name_;
};

}  // namespace mp
