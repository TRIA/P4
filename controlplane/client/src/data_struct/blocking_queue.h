// Copied from https://vorbrodt.blog/2019/02/03/blocking-queue/
// Changes in "pop" signature to return "T &item"

/* We need two semaphores: one to count the open slots, and another to count the full slots.
 * The open-slot semaphore starts with a count equal to the size of the queue.
 * The full-slot semaphore starts with a count of zero.
 * A push operation waits on the open-slot semaphore and signals the full-slot semaphore.
 * A pop operation waits on the full-slot semaphore and signals the open-slot semaphore.
 */

// #pragma once

#include <mutex>
// #include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include "data_struct/semaphore.h"

template <typename T>
class blocking_queue
{
public:
  blocking_queue(unsigned int size)
      : m_size(size), m_pushIndex(0), m_popIndex(0), m_count(0),
        m_data((T *)operator new(size * sizeof(T))),
        m_openSlots(size), m_fullSlots(0) {}

  blocking_queue(const blocking_queue &) = delete;
  blocking_queue(blocking_queue &&) = delete;
  blocking_queue &operator=(const blocking_queue &) = delete;
  blocking_queue &operator=(blocking_queue &&) = delete;

  ~blocking_queue()
  {
    while (m_count--)
    {
      m_data[m_popIndex].~T();
      m_popIndex = ++m_popIndex % m_size;
    }
    operator delete(m_data);
  }

  void push(const T &item)
  {
    m_openSlots.wait();
    {
      std::lock_guard<std::mutex> lock(m_cs);
      new (m_data + m_pushIndex) T(item);
      m_pushIndex = ++m_pushIndex % m_size;
      ++m_count;
    }
    m_fullSlots.post();
  }

  void pop(T &item)
  {
    m_fullSlots.wait();
    {
      std::lock_guard<std::mutex> lock(m_cs);
      item = m_data[m_popIndex];
      m_data[m_popIndex].~T();
      m_popIndex = ++m_popIndex % m_size;
      --m_count;
    }
    m_openSlots.post();
  }

  bool empty()
  {
    std::lock_guard<std::mutex> lock(m_cs);
    return m_count == 0;
  }

private:
  unsigned int m_size;
  unsigned int m_pushIndex;
  unsigned int m_popIndex;
  unsigned int m_count;
  T *m_data;

  // boost::interprocess::interprocess_semaphore m_openSlots;
  // boost::interprocess::interprocess_semaphore m_fullSlots;
  Semaphore m_openSlots;
  Semaphore m_fullSlots;
  std::mutex m_cs;
};