/*******************************************************************************
Description:  A wrapper that convert an LLVM ScheduleDAG to an OptSched
              DataDepGraph.
Author:       Max Shawabkeh
Created:      Apr. 2011
Last Update:  Mar. 2017
*******************************************************************************/

#ifndef OPTSCHED_DAG_WRAPPER_H
#define OPTSCHED_DAG_WRAPPER_H

#include "llvm/CodeGen/OptSched/OptSchedMachineWrapper.h"
#include <vector>
#include <map>
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/CodeGen/OptSched/basic/data_dep.h"
#include "llvm/CodeGen/MachineScheduler.h"

namespace opt_sched {

class LLVMDataDepGraph : public DataDepGraph {
  public:
    LLVMDataDepGraph(llvm::MachineSchedContext* context,
                     llvm::ScheduleDAGMILive* llvmDag,
                     LLVMMachineModel* machMdl,
                     LATENCY_PRECISION ltncyPrcsn,
                     bool treatOrderDepsAsDataDeps,
                     int maxDagSizeForPrcisLtncy);
    ~LLVMDataDepGraph() {}

    // Returns a pointer to the SUnit at a given node index.
    llvm::SUnit* GetSUnit(size_t index) const;

    // Counts the maximum number of virtual registers of each type used by the
    // graph.
    virtual void CountDefs(RegisterFile regFiles[]);
    // Counts the number of definitions and usages for each register and updates
    // instructions to point to the registers they define/use.
    virtual void AddDefsAndUses(RegisterFile regFiles[]);
    // Creates the output edges missing from LLVM's ScheduleDAG.
    virtual void AddOutputEdges();

  protected:
    // A convenience machMdl_ pointer casted to LLVMMachineModel*.
    LLVMMachineModel* llvmMachMdl_;
    // A reference to the nodes of the LLVM DAG.
    std::vector<llvm::SUnit>& llvmNodes_;
    // An reference to the LLVM scheduler root class, used to access environment
    // and target info.
    llvm::MachineSchedContext* context_;
    // An reference to the LLVM Schedule DAG.
    llvm::ScheduleDAGMILive* schedDag_;
		// Precision of latency info
    LATENCY_PRECISION ltncyPrcsn_;
    // An option to treat data dependencies of type ORDER as data dependencies
    bool treatOrderDepsAsDataDeps_;
    // The maximum DAG size to be scheduled using precise latency information
    int maxDagSizeForPrcisLtncy_;
		// LLVM object with information about the machine we are targeting
		const llvm::TargetMachine& target_;
    // Check is SUnit is a root node
    bool isRootNode(const llvm::SUnit& unit);
    // Check is SUnit is a leaf node
    bool isLeafNode(const llvm::SUnit& unit);

    // Converts the LLVM nodes saved in llvmNodes_ to opt_sched::DataDepGraph.
    // Should be called only once, by the constructor.
    void ConvertLLVMNodes_();
    // Returns the register type of a given machine instruction result.
    int GetRegisterType_(const unsigned resNo) const;

    // Holds a register live range, mapping a producer to a set of consumers.
    struct LiveRange {
      // The node which defines the register tracked by this live range.
      SchedInstruction* producer;
      // The nodes which use the register tracked by this live range.
      std::vector<SchedInstruction*> consumers;
    };
};

} // end namespace opt_sched

#endif