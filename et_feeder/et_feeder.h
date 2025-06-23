#pragma once

#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "third_party/utils/protoio.hh"
#include "et_feeder/et_feeder_node.h"

namespace Chakra {
struct CompareNodes: public std::binary_function<std::shared_ptr<ETFeederNode>, std::shared_ptr<ETFeederNode>, bool>
{
  bool operator()(const std::shared_ptr<ETFeederNode> lhs, const std::shared_ptr<ETFeederNode> rhs) const
  {
    return lhs->getChakraNode()->id() > rhs->getChakraNode()->id();
  }
};

class ETFeeder {
 public:
  ETFeeder(std::string filename);
  ~ETFeeder();

  void addNode(std::shared_ptr<ETFeederNode> node);
  void removeNode(uint64_t node_id);
  bool hasNodesToIssue();
  std::shared_ptr<ETFeederNode> getNextIssuableNode();
  void pushBackIssuableNode(uint64_t node_id);
  std::shared_ptr<ETFeederNode> lookupNode(uint64_t node_id);
  void freeChildrenNodes(uint64_t node_id);

  // jinting: debug
  // how many nodes were ever loaded into the trace
  size_t getTotalTraceNodes() const { return total_trace_nodes_; }
  // how many are currently in the graph (i.e. registered but not issued)
  size_t getDepGraphSize()   const { return dep_graph_.size(); }
  // how many are currently ready-to-issue
  size_t getFreeQueueSize()  const { return dep_free_node_queue_.size(); }

 private:
  void readGlobalMetadata();
  std::shared_ptr<ETFeederNode> readNode();
  void readNextWindow();
  void resolveDep();
  bool checkForOrphanedDependencies();
  void repairOrphanedDependencies();

  ProtoInputStream trace_;
  const uint32_t window_size_;
  bool et_complete_;

  std::unordered_map<uint64_t, std::shared_ptr<ETFeederNode>> dep_graph_{};
  std::unordered_set<uint64_t> dep_free_node_id_set_{};
  std::priority_queue<std::shared_ptr<ETFeederNode>, std::vector<std::shared_ptr<ETFeederNode>>, CompareNodes> dep_free_node_queue_{};
  std::unordered_set<std::shared_ptr<ETFeederNode>> dep_unresolved_node_set_{};

  size_t total_trace_nodes_{0};
};

} // namespace Chakra
