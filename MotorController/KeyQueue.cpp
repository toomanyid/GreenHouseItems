#include "KeyQueue.h"

volatile Key KeyQueue::queue[KeyQueue::CAPACITY];
volatile int KeyQueue::head = 0;
volatile int KeyQueue::tail = 0;
volatile int KeyQueue::count = 0;

void IRAM_ATTR KeyQueue::push(Key k) {
  if (k == Key::None) return;
  int c = count;
  if (c >= CAPACITY) return;
  queue[tail] = k;
  tail = (tail + 1) % CAPACITY;
  count = c + 1;
}

Key KeyQueue::pop() {
  int c = count;
  if (c <= 0) return Key::None;
  Key k = queue[head];
  head = (head + 1) % CAPACITY;
  count = c - 1;
  return k;
}

bool KeyQueue::isEmpty() {
  return count == 0;
}
