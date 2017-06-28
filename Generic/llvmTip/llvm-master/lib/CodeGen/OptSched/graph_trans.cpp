#include "llvm/CodeGen/OptSched/basic/graph_trans.h"
#include "llvm/CodeGen/OptSched/generic/bit_vector.h"
#include "llvm/CodeGen/OptSched/generic/logger.h"

namespace opt_sched {

GraphTrans::GraphTrans(DataDepGraph* dataDepGraph) {
  assert(dataDepGraph != NULL);

  SetDataDepGraph(dataDepGraph);
  SetNumNodesInGraph(dataDepGraph->GetInstCnt());
}

std::unique_ptr<GraphTrans> GraphTrans::CreateGraphTrans(TRANS_TYPE type, DataDepGraph* dataDepGraph) {
  switch(type) {
    // Create equivalence detection graph transformation.
    case TT_EQDECT:
      return std::unique_ptr<GraphTrans> (new EquivDectTrans(dataDepGraph));
  }
}

bool GraphTrans::AreNodesIndep_(SchedInstruction* inst1, SchedInstruction* inst2) {
 // The nodes are independent if there is no path from srcInst to dstInst.
 if (!inst1->IsRcrsvPrdcsr(inst2) && !inst1->IsRcrsvScsr(inst2)) {
  #ifdef IS_DEBUG_GRAPH_TRANS
  Logger::Info("Nodes %d and %d are independent", inst1->GetNum(), inst2->GetNum());
  #endif
  return true;
 }
 else
   return false;
}

FUNC_RESULT EquivDectTrans::ApplyTrans() {
  InstCount numNodes = GetNumNodesInGraph();
  DataDepGraph* graph = GetDataDepGraph();
  #ifdef IS_DEBUG_GRAPH_TRANS
  Logger::Info("Applying trans");
  #endif

  std::list<InstCount> nodes;
  // Initialize list of nodes.
  for (InstCount n = 0; n < numNodes; n++)
    nodes.push_back(n);

  std::list<InstCount>::iterator start, next;
  start = next = nodes.begin();
  // After we add an edge between two equivalent instructions
  // it will invalidate the equal predecessor condition for
  // future nodes. Therefore we should wait until we have checked all
  // potentially equivalent nodes before adding edges between them.
  std::vector<std::pair<InstCount, InstCount>> edgesToAdd;
  while (start != nodes.end()) {
    edgesToAdd.clear();
    next++;

    while (next != nodes.end()) {
      SchedInstruction* srcInst = graph->GetInstByIndx(*start);
      SchedInstruction* dstInst = graph->GetInstByIndx(*next);
      #ifdef IS_DEBUG_GRAPH_TRANS
      Logger::Info("Checking nodes %d:%d", *start, *next);
      #endif

      if (NodesAreEquiv_(srcInst, dstInst)) {
        #ifdef IS_DEBUG_GRAPH_TRANS
        Logger::Info("Nodes %d and %d are equivalent", *start, *next);
        #endif
        edgesToAdd.push_back(std::make_pair(*start, *next));
        nodes.erase(start);
        start = next;
      }
      next++;
    }
    // Add edges, we have found all nodes that are equivalent to the original "start"
    for (InstCount i = 0; i < edgesToAdd.size(); i++) {
      SchedInstruction* from = graph->GetInstByIndx(edgesToAdd[i].first);
      SchedInstruction* to = graph->GetInstByIndx(edgesToAdd[i].second);
      graph->CreateEdge(from, to, 0, DEP_OTHER);
    }
    start++;
    next = start;
  }
  return RES_SUCCESS;
}

bool EquivDectTrans::NodesAreEquiv_(SchedInstruction* srcInst, SchedInstruction* dstInst) {
  if (srcInst->GetIssueType() != dstInst->GetIssueType())
    return false;

	if (!srcInst->IsScsrEquvlnt(dstInst) || !srcInst->IsPrdcsrEquvlnt(dstInst))
      return false;

  // All tests passed return true
  return true;
}

} // end namespace opt_sched
