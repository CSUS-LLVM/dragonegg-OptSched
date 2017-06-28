/*******************************************************************************
Description:  Implement graph transformations to be applied before scheduling.
Author:       Austin Kerbow
Created:      June. 2017 
Last Update:  June. 2017
*******************************************************************************/

#ifndef OPTSCHED_BASIC_GRAPH_TRANS_H
#define OPTSCHED_BASIC_GRAPH_TRANS_H

#include "llvm/CodeGen/OptSched/basic/data_dep.h"
#include "llvm/CodeGen/OptSched/generic/defines.h"
#include "llvm/CodeGen/OptSched/generic/lnkd_lst.h"
#include <memory>
#include <list>

namespace opt_sched {

// Types of graph transformations.
enum TRANS_TYPE {
  TT_EQDECT = 0
};

// An abstract graph transformation class.
class GraphTrans {

  public:
    GraphTrans(DataDepGraph* dataDepGraph);
    virtual ~GraphTrans() {};
    
    // Create a graph transformation of the specified type.
    static std::unique_ptr<GraphTrans> CreateGraphTrans(TRANS_TYPE type, DataDepGraph* dataDepGraph);
    
    // Apply the graph transformation to the DataDepGraph.
    virtual FUNC_RESULT ApplyTrans() = 0;

    DataDepGraph* GetDataDepGraph() const;
    void SetDataDepGraph(DataDepGraph* dataDepGraph);

    InstCount GetNumNodesInGraph() const;
    void SetNumNodesInGraph(InstCount numNodesInGraph);

  protected:
    // Find independent nodes in the graph. Nodes are independent if
    // no path exists between them.
    bool AreNodesIndep_(SchedInstruction* inst1, SchedInstruction* inst2);

  private:
    // A pointer to the graph.
    DataDepGraph* dataDepGraph_;
    // The total number of nodes in the graph.
    InstCount numNodesInGraph_;
};

// The equivalence detection graph transformation. If two independent
// nodes are equivalent, create an edge between them.
class EquivDectTrans : public GraphTrans {
  public:
    EquivDectTrans(DataDepGraph* dataDepGraph);

    FUNC_RESULT ApplyTrans() override;

    private:
      // Return true if the nodes are equivalent and we can create an edge between them.
      bool NodesAreEquiv_(SchedInstruction* srcInst, SchedInstruction* dstInst);
};

inline DataDepGraph* GraphTrans::GetDataDepGraph() const {return dataDepGraph_;}
inline void GraphTrans::SetDataDepGraph(DataDepGraph* dataDepGraph) {dataDepGraph_ = dataDepGraph;}

inline InstCount GraphTrans::GetNumNodesInGraph() const {return numNodesInGraph_;}
inline void GraphTrans::SetNumNodesInGraph(InstCount numNodesInGraph) {numNodesInGraph_ = numNodesInGraph;}

inline EquivDectTrans::EquivDectTrans(DataDepGraph* dataDepGraph) : GraphTrans(dataDepGraph) {}

} // end namespace opt_sched

#endif
