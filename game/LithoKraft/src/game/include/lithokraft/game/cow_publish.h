// Lock-free copy-on-write publication of an immutable value (LF line endings).
//
// Replaces std::atomic<std::shared_ptr<const T>>, which every mainstream
// standard library implements with an internal lock. The current value is a
// raw const T* published with one CAS; readers never take a reference count.
//
// Reclamation: a reader pins (one fetch_add on pins_) for the few
// instructions it touches the value, then unpins. A writer that replaces the
// value pushes the old one onto a retired stack. Retired values are freed
// once a reclaim pass sees no pinned reader, taking the stack *before*
// checking the pins:
//   every reader that can still hold a retired value loaded it before it was
//   unpublished, so its pin precedes the take, which precedes the pin check
//   (all seq_cst). A zero count therefore proves nobody holds anything taken.
// If readers are pinned, the taken values go back on the stack for a later
// pass; the destructor frees whatever is left. Pins cover lookups and
// copies only, never slow work, so zero-pin moments are frequent.
//
// While a writer stays pinned, the value it copied cannot be freed, so its
// address cannot be reused and the publishing CAS has no ABA window.
#pragma once

#include <atomic>
#include <cstddef>
#include <utility>

namespace lithokraft {

template <class T>
class CowPublished {
 public:
  explicit CowPublished(T* initial) : cur_(initial) {}
  CowPublished(const CowPublished&) = delete;
  CowPublished& operator=(const CowPublished&) = delete;
  ~CowPublished() {
    delete cur_.load(std::memory_order_relaxed);
    freeChain(retired_.load(std::memory_order_relaxed));
  }

  /// Runs f(const T&) with the current value pinned and returns its result.
  template <class F>
  decltype(auto) read(F&& f) const {
    Pin pin(pins_);
    return std::forward<F>(f)(*cur_.load());
  }

  /// Copy-on-write update: make(const T&) returns a new T* built from the
  /// current value, or nullptr when that value needs no change. It runs
  /// pinned, so keep it to the copy itself. Retries on a lost race (make is
  /// called again on the newer value).
  template <class Make>
  void update(Make&& make) {
    for (;;) {
      Pin pin(pins_);
      const T* snap = cur_.load();
      T* next = make(*snap);
      if (!next) return;
      const T* expected = snap;
      if (cur_.compare_exchange_strong(expected, next)) {
        retire(snap);
        pin.release();
        reclaim();
        return;
      }
      delete next;
    }
  }

  size_t retiredCount() const {
    size_t n = 0;
    for (Node* p = retired_.load(); p; p = p->next) ++n;
    return n;
  }

 private:
  struct Node {
    const T* value;
    Node* next;
  };
  struct Pin {
    std::atomic<size_t>& c;
    bool held = true;
    explicit Pin(std::atomic<size_t>& c_) : c(c_) { c.fetch_add(1); }
    void release() { if (held) { c.fetch_sub(1); held = false; } }
    ~Pin() { release(); }
  };

  void retire(const T* old) {
    Node* n = new Node{old, retired_.load()};
    while (!retired_.compare_exchange_weak(n->next, n)) {}
  }
  void reclaim() {
    Node* taken = retired_.exchange(nullptr);
    if (!taken) return;
    if (pins_.load() == 0) { freeChain(taken); return; }
    Node* tail = taken;
    while (tail->next) tail = tail->next;
    tail->next = retired_.load();
    while (!retired_.compare_exchange_weak(tail->next, taken)) {}
  }
  static void freeChain(Node* p) {
    while (p) { Node* n = p->next; delete p->value; delete p; p = n; }
  }

  std::atomic<const T*> cur_;
  std::atomic<Node*> retired_{nullptr};
  mutable std::atomic<size_t> pins_{0};
};

}  // namespace lithokraft
