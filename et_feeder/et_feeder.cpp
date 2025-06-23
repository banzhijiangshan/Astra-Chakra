#include "et_feeder/et_feeder.h"

using namespace std;
using namespace Chakra;

ETFeeder::ETFeeder(string filename)
  : trace_(filename), window_size_(4096 * 256), et_complete_(false) {
  readGlobalMetadata();
  readNextWindow();

  // checkForOrphanedDependencies();
  repairOrphanedDependencies();
}

ETFeeder::~ETFeeder() {
}

void ETFeeder::addNode(shared_ptr<ETFeederNode> node) {
  dep_graph_[node->getChakraNode()->id()] = node;
}

void ETFeeder::removeNode(uint64_t node_id) {
  dep_graph_.erase(node_id);

  if (!et_complete_ && dep_free_node_queue_.size() < window_size_) {
    readNextWindow();
  }
}

bool ETFeeder::hasNodesToIssue() {
  return !(dep_graph_.empty() && dep_free_node_queue_.empty());
}

shared_ptr<ETFeederNode> ETFeeder::getNextIssuableNode() {

  if (!dep_free_node_queue_.empty()) {
    auto node = dep_free_node_queue_.top();
    dep_free_node_id_set_.erase(node->getChakraNode()->id());
    dep_free_node_queue_.pop();
    return node;
  } else {
    return nullptr;
  }
}

void ETFeeder::pushBackIssuableNode(uint64_t node_id) {
  shared_ptr<ETFeederNode> node = dep_graph_[node_id];
  dep_free_node_id_set_.emplace(node_id);
  dep_free_node_queue_.emplace(node);
}

shared_ptr<ETFeederNode> ETFeeder::lookupNode(uint64_t node_id) {
  return dep_graph_[node_id];
}

void ETFeeder::freeChildrenNodes(uint64_t node_id) {
  shared_ptr<ETFeederNode> node = dep_graph_[node_id];
  for (auto child: node->getChildren()) {
    auto child_chakra = child->getChakraNode();
    for (auto it = child_chakra->mutable_data_deps()->begin();
        it != child_chakra->mutable_data_deps()->end();
        ++it) {
      if (*it == node_id) {
        child_chakra->mutable_data_deps()->erase(it);
        break;
      }
    }
    if (child_chakra->data_deps().size() == 0) {
      dep_free_node_id_set_.emplace(child_chakra->id());
      dep_free_node_queue_.emplace(child);
    }
  }
}

void ETFeeder::readGlobalMetadata() {
  shared_ptr<ChakraProtoMsg::GlobalMetadata> pkt_msg = make_shared<ChakraProtoMsg::GlobalMetadata>();
  trace_.read(*pkt_msg);
}

shared_ptr<ETFeederNode> ETFeeder::readNode() {
  shared_ptr<ChakraProtoMsg::Node> pkt_msg = make_shared<ChakraProtoMsg::Node>();
  if (!trace_.read(*pkt_msg)) {
    return nullptr;
  }
  // uint64_t current_node_id = pkt_msg->id();
  // std::cout << "[ETFeeder::readNode] Read node ID: " << current_node_id << std::endl;
  // std::cout << "  Name: " << pkt_msg->name() << std::endl;
  // std::cout << "  Type: " << pkt_msg->type() << std::endl;
  // std::cout << "  Data deps count: " << pkt_msg->data_deps_size() << std::endl;
  // for (int i = 0; i < pkt_msg->data_deps_size(); ++i) {
  //   std::cout << "    Depends on: " << pkt_msg->data_deps(i) << std::endl;
  // }

  shared_ptr<ETFeederNode> node = make_shared<ETFeederNode>(pkt_msg);

  bool dep_unresolved = false;
  for (int i = 0; i < pkt_msg->data_deps_size(); ++i) {
    auto parent_node = dep_graph_.find(pkt_msg->data_deps(i));
    if (parent_node != dep_graph_.end()) {
      parent_node->second->addChild(node);
    } else {
      dep_unresolved = true;
      node->addDepUnresolvedParentID(pkt_msg->data_deps(i));
    }
  }

  if (dep_unresolved) {
    dep_unresolved_node_set_.emplace(node);
  }

  return node;
}

void ETFeeder::resolveDep() {
  for (auto it = dep_unresolved_node_set_.begin();
      it != dep_unresolved_node_set_.end();) {
    shared_ptr<ETFeederNode> node = *it;
    vector<uint64_t> dep_unresolved_parent_ids = node->getDepUnresolvedParentIDs();
    for (auto inner_it = dep_unresolved_parent_ids.begin();
        inner_it != dep_unresolved_parent_ids.end();) {
      auto parent_node = dep_graph_.find(*inner_it);
      if (parent_node != dep_graph_.end()) {
        parent_node->second->addChild(node);
        inner_it = dep_unresolved_parent_ids.erase(inner_it);
      } else {
        ++inner_it;
      }
    }
    if (dep_unresolved_parent_ids.size() == 0) {
      it = dep_unresolved_node_set_.erase(it);
    } else {
      node->setDepUnresolvedParentIDs(dep_unresolved_parent_ids);
      ++it;
    }
  }
}

void ETFeeder::readNextWindow() {
  uint32_t num_read = 0;
  uint32_t last_node_id = 0;
  do {
    shared_ptr<ETFeederNode> new_node = readNode();
    // if (new_node != nullptr) {
    //   last_node_id = new_node->getChakraNode()->id();
    // }
    if (new_node == nullptr) {
      // std::cout << "[ETFeeder::readNextWindow] read complete, last node id="
      //           << last_node_id << "\n";
      et_complete_ = true;
      break;
    }

    addNode(new_node);
    ++num_read;

    resolveDep();
  } while ((num_read < window_size_)
      || (dep_unresolved_node_set_.size() != 0));

  // ADD THIS: Report unresolved dependencies when window completes
  // if (!dep_unresolved_node_set_.empty()) {
  //   std::cout << "[ETFeeder::readNextWindow] WARNING: " << dep_unresolved_node_set_.size() 
  //             << " nodes still have unresolved dependencies!" << std::endl;
    
  //   for (auto node : dep_unresolved_node_set_) {
  //     auto unresolved_ids = node->getDepUnresolvedParentIDs();
  //     std::cout << "  Node " << node->getChakraNode()->id() 
  //               << " waiting for dependencies: ";
  //     for (auto dep_id : unresolved_ids) {
  //       std::cout << dep_id << " ";
  //     }
  //     std::cout << std::endl;
  //   }
  // }

  for (auto node_id_node: dep_graph_) {
    uint64_t node_id = node_id_node.first;
    shared_ptr<ETFeederNode> node = node_id_node.second;
    if ((dep_free_node_id_set_.count(node_id) == 0)
        && (node->getChakraNode()->data_deps().size() == 0)) {
      dep_free_node_id_set_.emplace(node_id);
      dep_free_node_queue_.emplace(node);
    }
  }
}

// bool ETFeeder::checkForOrphanedDependencies() {
//   if (!dep_unresolved_node_set_.empty()) {
//     std::cout << "[ETFeeder] CRITICAL: Found " << dep_unresolved_node_set_.size() 
//               << " nodes with missing dependencies after trace completion!" << std::endl;
    
//     std::set<uint64_t> missing_node_ids;
//     for (auto node : dep_unresolved_node_set_) {
//       auto unresolved_ids = node->getDepUnresolvedParentIDs();
//       std::cout << "  Node " << node->getChakraNode()->id() 
//                 << " (" << node->getChakraNode()->name() << ") waiting for: ";
//       for (auto dep_id : unresolved_ids) {
//         std::cout << dep_id << " ";
//         missing_node_ids.insert(dep_id);
//       }
//       std::cout << std::endl;
//     }
    
//     std::cout << "[ETFeeder] Missing node IDs: ";
//     for (auto missing_id : missing_node_ids) {
//       std::cout << missing_id << " ";
//     }
//     std::cout << std::endl;
//     return true;
//   }
//   else {
//     return false;
//   }
// }

void ETFeeder::repairOrphanedDependencies() {
  if (dep_unresolved_node_set_.empty()) {
    // std::cout << "[ETFeeder] No orphaned dependencies to repair." << std::endl;
    return;
  }

  // std::cout << "[ETFeeder] REPAIR: Cleaning up " << dep_unresolved_node_set_.size() 
  //           << " nodes with missing dependencies..." << std::endl;
  
  std::set<uint64_t> missing_node_ids;

  for (auto node : dep_unresolved_node_set_) {
    auto unresolved_ids = node->getDepUnresolvedParentIDs();
    for (auto dep_id : unresolved_ids) {
      missing_node_ids.insert(dep_id);
    }
  }
  
  // std::cout << "[ETFeeder] REPAIR: Found " << missing_node_ids.size() 
  //           << " missing dependency IDs: ";
  // for (auto missing_id : missing_node_ids) {
  //   std::cout << missing_id << " ";
  // }
  // std::cout << std::endl;

  // Remove missing dependencies from ALL nodes in the graph
  for (auto& node_pair : dep_graph_) {
    auto node = node_pair.second;
    auto chakra_node = node->getChakraNode();
    
    auto data_deps = chakra_node->mutable_data_deps();
    bool deps_modified = false;
    
    for (auto it = data_deps->begin(); it != data_deps->end();) {
      if (missing_node_ids.count(*it) > 0) {
        // std::cout << "[ETFeeder] REPAIR: Removing missing dependency " << *it 
        //           << " from node " << chakra_node->id() << std::endl;
        it = data_deps->erase(it);
        deps_modified = true;
      } else {
        ++it;
      }
    }
    
    // If dependencies were removed and node now has no deps, make it issuable
    if (deps_modified && chakra_node->data_deps().size() == 0) {
      if (dep_free_node_id_set_.count(chakra_node->id()) == 0) {
        // std::cout << "[ETFeeder] REPAIR: Node " << chakra_node->id() 
        //           << " is now dependency-free, adding to issuable queue" << std::endl;
        dep_free_node_id_set_.emplace(chakra_node->id());
        dep_free_node_queue_.emplace(node);
      }
    }
  }

  // Clear the unresolved nodes
  dep_unresolved_node_set_.clear();
  
  // std::cout << "[ETFeeder] REPAIR: Cleanup complete. Free queue now has " 
  //           << dep_free_node_queue_.size() << " nodes ready to execute." << std::endl;
}