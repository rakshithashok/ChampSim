#ifndef ADAPTIVE_BW_H
#define ADAPTIVE_BW_H

#include <cstdint>
#include <unordered_set>

#include "address.h"
#include "champsim.h"
#include "modules.h"
#include "msl/lru_table.h"

struct adaptive_bw : public champsim::modules::prefetcher {
  struct tracker_entry {
    champsim::address ip{};
    champsim::block_number last{};
    champsim::block_number::difference_type stride{};
    auto index() const { using namespace champsim::data::data_literals; return ip.slice_upper<2_b>(); }
    auto tag() const { using namespace champsim::data::data_literals; return ip.slice_upper<2_b>(); }
  };

  static constexpr std::size_t TRACKER_SETS = 256;
  static constexpr std::size_t TRACKER_WAYS = 4;
  static constexpr int MAX_DISTANCE = 8;
  champsim::msl::lru_table<tracker_entry> table{TRACKER_SETS, TRACKER_WAYS};
  std::unordered_set<uint64_t> outstanding;
  std::unordered_set<uint64_t> resident_prefetches;
  unsigned distance = 1;
  uint64_t issued = 0, useful = 0, late = 0, unused = 0;
  double accuracy_ewma = 0.5;

public:
  using champsim::modules::prefetcher::prefetcher;
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_final_stats();
};

#endif
