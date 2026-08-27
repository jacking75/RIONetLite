#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace rionetlite::detail {

struct ProcessorLocation final {
  std::uint16_t group = 0;
  std::uint8_t number = 0;
  std::uint16_t numa_node = 0;

  friend bool operator==(const ProcessorLocation&,
                         const ProcessorLocation&) = default;
};

inline std::vector<ProcessorLocation> OrderProcessorsForAffinity(
    const std::span<const ProcessorLocation> processors) {
  std::vector<std::uint16_t> nodes;
  nodes.reserve(processors.size());
  for (const auto& processor : processors) {
    if (std::find(nodes.begin(), nodes.end(), processor.numa_node) ==
        nodes.end()) {
      nodes.push_back(processor.numa_node);
    }
  }

  std::vector<ProcessorLocation> ordered;
  ordered.reserve(processors.size());
  for (std::size_t ordinal = 0; ordered.size() < processors.size(); ++ordinal) {
    for (const auto node : nodes) {
      std::size_t current = 0;
      for (const auto& processor : processors) {
        if (processor.numa_node != node) continue;
        if (current++ == ordinal) {
          ordered.push_back(processor);
          break;
        }
      }
    }
  }
  return ordered;
}

}  // namespace rionetlite::detail
