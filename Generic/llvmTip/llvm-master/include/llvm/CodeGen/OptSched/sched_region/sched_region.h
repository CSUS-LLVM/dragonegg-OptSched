/*******************************************************************************
Description:  Implements an abstract base class for representing scheduling
              regions.
Author:       Ghassan Shobaki
Created:      Apr. 2005
Last Update:  Apr. 2011
*******************************************************************************/

#ifndef OPTSCHED_SCHED_REGION_SCHED_REGION_H
#define OPTSCHED_SCHED_REGION_SCHED_REGION_H

#include "llvm/CodeGen/OptSched/generic/defines.h"
#include "llvm/CodeGen/OptSched/generic/lnkd_lst.h"
#include "llvm/CodeGen/OptSched/basic/sched_basic_data.h"
// For DataDepGraph, LB_ALG.
#include "llvm/CodeGen/OptSched/basic/data_dep.h"
// For Enumerator, LengthCostEnumerator, EnumTreeNode and Pruning.
#include "llvm/CodeGen/OptSched/enum/enumerator.h"

namespace opt_sched {

// How to compare cost.
// TODO(max): Elaborate.
enum COST_COMP_MODE {
  // Dynamically.
  CCM_DYNMC,
  // Statically.
  CCM_STTC
};

class ListScheduler;

class SchedRegion {
  public:
    // TODO(max): Document.
    SchedRegion(MachineModel* machMdl,
                DataDepGraph* dataDepGraph,
                long rgnNum,
                int16_t sigHashSize,
                LB_ALG lbAlg,
                SchedPriorities hurstcPrirts,
                SchedPriorities enumPrirts,
                bool vrfySched,
                Pruning prune);
    // Destroys the region. Must be overriden by child classes.
    virtual ~SchedRegion() {}

    // Returns the dependence graph of this region.
    inline DataDepGraph* GetDepGraph() { return dataDepGraph_; }
    // Returns the lower bound on the cost of this region.
    inline int GetCostLwrBound() { return costLwrBound_; }
    // Returns the best cost found so far for this region.
    inline InstCount GetBestCost() { return bestCost_; }

    // TODO(max): Document.
    virtual FUNC_RESULT FindOptimalSchedule(bool useFileBounds,
                                            Milliseconds rgnTimeout,
                                            Milliseconds lngthTimeout,
                                            bool& isHurstcOptml,
                                            InstCount& bestCost,
                                            InstCount& bestSchedLngth,
                                            InstCount& hurstcCost,
                                            InstCount& hurstcSchedLngth,
                                            InstSchedule*& bestSched);

    // External abstract functions.

    // TODO(max): Document.
    virtual FUNC_RESULT BuildFromFile() = 0;
    // TODO(max): Document.
    virtual int CmputCostLwrBound() = 0;
    // TODO(max): Document.
    virtual InstCount UpdtOptmlSched(InstSchedule* crntSched,
                                     LengthCostEnumerator* enumrtr) = 0;
    // TODO(max): Document.
    virtual bool ChkCostFsblty(InstCount trgtLngth,
                               EnumTreeNode* treeNode) = 0;
    // TODO(max): Document.
    virtual void SchdulInst(SchedInstruction* inst,
                            InstCount cycleNum,
                            InstCount slotNum,
                            bool trackCnflcts) = 0;
    // TODO(max): Document.
    virtual void UnschdulInst(SchedInstruction* inst,
                              InstCount cycleNum,
                              InstCount slotNum,
                              EnumTreeNode* trgtNode) = 0;
    // TODO(max): Document.
    virtual void SetSttcLwrBounds(EnumTreeNode* node) = 0;

    // Do region-specific checking for the legality of scheduling the
    // given instruction in the current issue slot  
    virtual bool ChkInstLglty(SchedInstruction* inst) = 0;

    virtual void InitForSchdulng() = 0;

    virtual bool ChkSchedule_(InstSchedule* bestSched, InstSchedule* lstSched) = 0;

  protected:
    // The dependence graph of this region.
    DataDepGraph* dataDepGraph_;
    // The machine model used by this region.
    MachineModel* machMdl_;
    // The number of this region.
    long rgnNum_;
    // The number of instructions in this region.
    InstCount instCnt_;
    // Whether to verify the schedule after calculating it.
    bool vrfySched_;

    // The best results found so far.
    InstCount bestCost_;
    InstCount bestSchedLngth_;
    // The nomal heuristic scheduling results.
    InstCount hurstcCost_;
    InstCount hurstcSchedLngth_;

    // The schedule currently used by the enumerator
    InstSchedule* enumCrntSched_;
    // The best schedule found by the enumerator so far
    InstSchedule* enumBestSched_;
    // The best schedule found so far (may be heuristic or enumerator generated)
    InstSchedule* bestSched_;

    // The absolute cost lower bound to be used as a ref for normalized costs.
    InstCount costLwrBound_;

    // TODO(max): Document.
    InstCount schedLwrBound_;
    // TODO(max): Document.
    InstCount schedUprBound_;
    // TODO(max): Document.
    InstCount abslutSchedUprBound_;

    // The algorithm to use for calculated lower bounds.
    LB_ALG lbAlg_;

    // list scheduling heuristics
    SchedPriorities hurstcPrirts_;
    // Scheduling heuristics to use when enumerating
    SchedPriorities enumPrirts_;

    // The pruning technique to use for this region.
    Pruning prune_;

    // Do we need to compute the graph's transitive closure?
    bool needTrnstvClsr_;

    // TODO(max): Document.
    int16_t sigHashSize_;

    // TODO(max): Document.
    InstCount crntCycleNum_;
    // TODO(max): Document.
    InstCount crntSlotNum_;

    // TODO(max): Document.
    InstSchedule* AllocNewSched_();
    // TODO(max): Document.
    void UseFileBounds_();

    // Top-level function for enumerative scheduling
    FUNC_RESULT Optimize_(Milliseconds startTime,
                          Milliseconds rgnTimeout,
                          Milliseconds lngthTimeout);
    // TODO(max): Document.
    void CmputLwrBounds_(bool useFileBounds);
    // TODO(max): Document.
    bool CmputUprBounds_(InstSchedule* lstSched, bool useFileBounds);
    // Handle the enumerator's result
    void HandlEnumrtrRslt_(FUNC_RESULT rslt,
                           InstCount trgtLngth);

    // TODO(max): Document.
    virtual void CmputAbslutUprBound_();

    // Internal abstract functions.

    // Compute the normalized cost.
    virtual InstCount CmputNormCost_(InstSchedule* sched,
                                     COST_COMP_MODE compMode,
                                     InstCount& execCost,
                                     bool trackCnflcts) = 0;
    // TODO(max): Document.
    virtual InstCount CmputCost_(InstSchedule* sched,
                                 COST_COMP_MODE compMode,
                                 InstCount& execCost,
                                 bool trackCnflcts) = 0;
    // TODO(max): Document.
    virtual void CmputSchedUprBound_() = 0;
    // TODO(max): Document.
    virtual Enumerator* AllocEnumrtr_(Milliseconds timeout) = 0;
    // Wrapper for the enumerator
    virtual FUNC_RESULT Enumerate_(Milliseconds startTime, Milliseconds rgnTimeout, Milliseconds lngthTimeout) = 0;
    // TODO(max): Document.
    virtual void FinishHurstc_() = 0;
    // TODO(max): Document.
    virtual void FinishOptml_() = 0;
    // TODO(max): Document.
    virtual ListScheduler* AllocLstSchdulr_() = 0;

    virtual bool EnableEnum_() = 0;

    // Prepares the region for being scheduled.
    virtual void SetupForSchdulng_() = 0;
};

} // end namespace opt_sched

#endif
