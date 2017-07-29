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
#include "llvm/CodeGen/OptSched/sched_region/sched_region.h"
#include <memory>
#include <list>

namespace opt_sched {

// Types of graph transformations.
enum TRANS_TYPE {
  TT_EQDECT = 0,
  TT_RPONSP = 1
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

    void SetDataDepGraph(DataDepGraph* dataDepGraph);

    void SetSchedRegion(SchedRegion* schedRegion);

    void SetNumNodesInGraph(InstCount numNodesInGraph);

  protected:
    // Find independent nodes in the graph. Nodes are independent if
    // no path exists between them.
    bool AreNodesIndep_(SchedInstruction* inst1, SchedInstruction* inst2);
		// Update the recursive predecessor and successor lists after adding an edge between A and B.
    void UpdatePrdcsrAndScsr_(SchedInstruction* nodeA, SchedInstruction* nodeB);
    
    DataDepGraph* GetDataDepGraph_() const;
    SchedRegion* GetSchedRegion_() const;
    InstCount GetNumNodesInGraph_() const;

  private:
    // A pointer to the graph.
    DataDepGraph* dataDepGraph_;
    // A pointer to the scheduling region.
    SchedRegion* schedRegion_;
    // The total number of nodes in the graph.
    InstCount numNodesInGraph_;
};

inline DataDepGraph* GraphTrans::GetDataDepGraph_() const {return dataDepGraph_;}
inline void GraphTrans::SetDataDepGraph(DataDepGraph* dataDepGraph) {dataDepGraph_ = dataDepGraph;}

inline SchedRegion* GraphTrans::GetSchedRegion_() const {return schedRegion_;}
inline void GraphTrans::SetSchedRegion(SchedRegion* schedRegion) {schedRegion_ = schedRegion;}

inline InstCount GraphTrans::GetNumNodesInGraph_() const {return numNodesInGraph_;}
inline void GraphTrans::SetNumNodesInGraph(InstCount numNodesInGraph) {numNodesInGraph_ = numNodesInGraph;}

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

inline EquivDectTrans::EquivDectTrans(DataDepGraph* dataDepGraph) : GraphTrans(dataDepGraph) {}

// Node superiority graph transformation while only considering register pressure.
class RPOnlyNodeSupTrans : public GraphTrans {
  public:
    RPOnlyNodeSupTrans(DataDepGraph* dataDepGraph);

    FUNC_RESULT ApplyTrans() override;

  private:
    // Return true if node A is superior to node B.
    bool NodeIsSuperior_(SchedInstruction* nodeA, SchedInstruction* nodeB);
    // Check if there is superiority involving nodes A and B. If yes, choose which edge to add.
    // Returns true if a superior edge was added.
    bool TryAddingSuperiorEdge_(SchedInstruction* nodeA, SchedInstruction* nodeB);
		// Add an edge from node A to B and update the graph.
    void AddSuperiorEdge_(SchedInstruction* nodeA, SchedInstruction* nodeB);
};

inline RPOnlyNodeSupTrans::RPOnlyNodeSupTrans(DataDepGraph* dataDepGraph) : GraphTrans(dataDepGraph) {}

} // end namespace opt_sched

#endif
