#pragma once

#include "common.h"

namespace tl {

namespace detail {

// Software CTA-local mbarrier state:
//   bits  0..30: arrivals in the current phase
//   bit      31: completed parity, initialized to 1
//   bits 32..63: expected arrivals per phase
TL_DEVICE unsigned int *mbarrier_count_word(uint64_t &smem_barrier) {
  return reinterpret_cast<unsigned int *>(&smem_barrier);
}

TL_DEVICE unsigned int *mbarrier_expected_word(uint64_t &smem_barrier) {
  return reinterpret_cast<unsigned int *>(&smem_barrier) + 1;
}

} // namespace detail

TL_DEVICE void mbarrier_init(uint64_t &smem_barrier, uint32_t arrive_count) {
  detail::mbarrier_count_word(smem_barrier)[0] = 0x80000000u;
  detail::mbarrier_expected_word(smem_barrier)[0] = arrive_count;
  __threadfence_block();
}

TL_DEVICE uint32_t mbarrier_try_wait(uint64_t &smem_barrier, int phase_bit) {
  uint32_t phase = static_cast<uint32_t>(phase_bit) & 1u;
  uint32_t observed = atomicAdd(detail::mbarrier_count_word(smem_barrier), 0u);
  return ((observed >> 31) & 1u) == phase;
}

TL_DEVICE void mbarrier_wait(uint64_t &smem_barrier, int phase_bit) {
  while (!mbarrier_try_wait(smem_barrier, phase_bit)) {
    asm volatile("" ::: "memory");
  }
  __threadfence_block();
}

TL_DEVICE void mbarrier_arrive(uint64_t &smem_barrier) {
  __threadfence_block();
  unsigned int *count_word = detail::mbarrier_count_word(smem_barrier);
  uint32_t old = atomicAdd(count_word, 1u);
  uint32_t old_count = old & 0x7fffffffu;
  uint32_t expected = detail::mbarrier_expected_word(smem_barrier)[0];
  if (old_count + 1u == expected) {
    atomicSub(count_word, expected);
    __threadfence_block();
    atomicXor(count_word, 0x80000000u);
  }
}

TL_DEVICE void mbarrier_arrive(uint64_t &smem_barrier, int cta_id,
                               uint32_t pred = 1) {
  (void)cta_id;
  if (pred) {
    mbarrier_arrive(smem_barrier);
  }
}

TL_DEVICE void mbarrier_expect_tx(uint64_t &smem_barrier,
                                  uint32_t transaction_bytes) {
  (void)smem_barrier;
  (void)transaction_bytes;
}

TL_DEVICE void mbarrier_arrive_expect_tx(uint64_t &smem_barrier,
                                         uint32_t transaction_bytes) {
  (void)transaction_bytes;
  mbarrier_arrive(smem_barrier);
}

TL_DEVICE void mbarrier_arrive_expect_tx(uint64_t &smem_barrier,
                                         uint32_t transaction_bytes,
                                         int cta_id, uint32_t pred) {
  (void)transaction_bytes;
  mbarrier_arrive(smem_barrier, cta_id, pred);
}

template <typename BarrierType = uint64_t>
TL_DEVICE void mbarrier_cp_async_arrive(BarrierType &smem_mbar) {
  mbarrier_arrive(*reinterpret_cast<uint64_t *>(&smem_mbar));
}

TL_DEVICE void fence_barrier_init() { __threadfence_block(); }

} // namespace tl
