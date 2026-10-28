#include "adaptive_bw.h"

#include <algorithm>
#include <iostream>

uint32_t adaptive_bw::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                               uint32_t metadata_in)
{
  const auto block = champsim::block_number{addr};
  const auto key = block.to<uint64_t>();

  // Lower-level prefetch traffic has no originating instruction stream to
  // learn from and must not be mistaken for a demand stride.
  if (type == access_type::PREFETCH)
    return metadata_in;

  // A demand arriving before the requested prefetch filled is late. A demand
  // hit on a prefetched block is counted by ChampSim's useful_prefetch signal.
  if (type != access_type::PREFETCH) {
    if (useful_prefetch) {
      ++useful;
      resident_prefetches.erase(key);
      accuracy_ewma = 0.9 * accuracy_ewma + 0.1;
    } else if (outstanding.erase(key) != 0) {
      ++late;
      accuracy_ewma *= 0.9;
    }
  }

  champsim::block_number::difference_type stride = 0;
  auto found = table.check_hit({ip, block, stride});
  if (found) {
    stride = champsim::offset(found->last, block);
    if (stride != 0 && stride == found->stride) {
      // Queue pressure throttles both the maximum lookahead and fill level.
      const auto pq = intern_->get_pq_occupancy_ratio();
      const double pq_pressure = pq.empty() ? 0.0 : *std::max_element(pq.begin(), pq.end());
      const double pressure = std::max(intern_->get_mshr_occupancy_ratio(), pq_pressure);
      if (pressure < 0.5 && accuracy_ewma >= 0.35)
        distance = std::min<unsigned>(MAX_DISTANCE, distance + 1);
      else if (pressure > 0.8 || accuracy_ewma < 0.2)
        distance = std::max<unsigned>(1, distance - 1);

      for (unsigned d = 1; d <= distance; ++d) {
        champsim::address pf_addr{champsim::block_number{block + stride * d}};
        if (!intern_->virtual_prefetch && champsim::page_number{pf_addr} != champsim::page_number{addr})
          break;
        const auto pf_key = champsim::block_number{pf_addr}.to<uint64_t>();
        if (outstanding.count(pf_key) || resident_prefetches.count(pf_key))
          continue;
        const bool fill_here = pressure < 0.65;
        if (prefetch_line(pf_addr, fill_here, metadata_in)) {
          outstanding.insert(pf_key);
          ++issued;
          // Bound bookkeeping if a workload generates a long stream of
          // requests that never return to this cache.
          if (outstanding.size() > 8192)
            outstanding.erase(outstanding.begin());
        }
      }
    }
  }
  table.fill({ip, block, stride});
  return metadata_in;
}

uint32_t adaptive_bw::prefetcher_cache_fill(champsim::address addr, long, long, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  if (champsim::address{evicted_addr} != champsim::address{}) {
    const auto evicted_key = champsim::block_number{evicted_addr}.to<uint64_t>();
    if (resident_prefetches.erase(evicted_key) != 0)
      ++unused;
  }
  if (prefetch) {
    const auto key = champsim::block_number{addr}.to<uint64_t>();
    outstanding.erase(key);
    resident_prefetches.insert(key);
    if (resident_prefetches.size() > 8192)
      resident_prefetches.erase(resident_prefetches.begin());
  }
  // Unused fills lower aggressiveness gradually, reducing cache pollution.
  if (unused > 0 && (unused % 16) == 0)
    accuracy_ewma = std::max(0.0, accuracy_ewma - 0.05);
  return metadata_in;
}

void adaptive_bw::prefetcher_final_stats()
{
  std::cout << "adaptive_bw prefetches issued: " << issued << ", useful: " << useful << ", late: " << late << ", unused: " << unused
            << ", final distance: " << distance << ", accuracy: " << (issued ? static_cast<double>(useful) / issued : 0.0) << std::endl;
}
