/*******************************************************************************
Description:  Defines a scheduling region for basic blocks whose scheduler takes
              into account the cost of spilled registers.
Author:       Ghassan Shobaki
Created:      Unknown
Last Update:  Apr. 2011
*******************************************************************************/

#ifndef OPTSCHED_SPILL_BB_SPILL_H
#define OPTSCHED_SPILL_BB_SPILL_H

#include "llvm/CodeGen/OptSched/generic/defines.h"
#include "llvm/CodeGen/OptSched/sched_region/sched_region.h"

namespace opt_sched {

class LengthCostEnumerator;
class EnumTreeNode;
class Register;
class RegisterFile;
class BitVector;

class BBWithSpill : public SchedRegion {
  private:
    LengthCostEnumerator* enumrtr_;

    InstCount crntSpillCost_;
    InstCount optmlSpillCost_;

    bool enblStallEnum_;
    int spillCostFactor_;
    int schedCostFactor_;

    int16_t regTypeCnt_;
    RegisterFile* regFiles_;

    // A bit vector indexed by register number indicating whether that
    // register is live
    BitVector* liveRegs_;

    // A bit vector indexed by physical register number indicating whether 
    // that physical register is live
    BitVector* livePhysRegs_;

    int entryInstCnt_;
    int exitInstCnt_;
    int schduldEntryInstCnt_;
    int schduldExitInstCnt_;
    int schduldInstCnt_;
    bool fixLivein_;
    bool fixLiveout_;

    InstCount* spillCosts_;
    InstCount* peakRegPressures_;
    InstCount crntStepNum_;
    InstCount peakSpillCost_;
    InstCount totSpillCost_;
    SPILL_COST_FUNCTION spillCostFunc_;
    bool chkSpillCostSum_;
    bool chkCnflcts_;
    int maxSpillCost_;
    bool trackLiveRangeLngths_;

    //Virtual Functions:
    //Given a schedule, compute the cost function value
    InstCount CmputNormCost_(InstSchedule* sched, COST_COMP_MODE compMode,
                               InstCount& execCost, bool trackCnflcts);
    InstCount CmputCost_(InstSchedule* sched, COST_COMP_MODE compMode,
                           InstCount& execCost, bool trackCnflcts);
    void CmputSchedUprBound_();
    Enumerator* AllocEnumrtr_(Milliseconds timeout);
    FUNC_RESULT Enumerate_(Milliseconds startTime, Milliseconds rgnDeadline, Milliseconds lngthDeadline);
    void SetupForSchdulng_();
    void FinishHurstc_();
    void FinishOptml_();
    void CmputAbslutUprBound_();
    ListScheduler* AllocLstSchdulr_();
    bool EnableEnum_();

    //BBWithSpill-specific Functions:
    InstCount CmputCostLwrBound_(InstCount schedLngth);
    InstCount CmputCostLwrBound_();
    void InitForCostCmputtn_();
    InstCount CmputDynmcCost_();

    void UpdateSpillInfoForSchdul_(SchedInstruction* inst, bool trackCnflcts);
    void UpdateSpillInfoForUnSchdul_(SchedInstruction* inst);
    void SetupPhysRegs_();
    void CmputCrntSpillCost_();
    bool ChkSchedule_(InstSchedule* bestSched, InstSchedule* lstSched);
    void CmputCnflcts_(InstSchedule* sched);

  public:
    BBWithSpill(MachineModel* machMdl, DataDepGraph* dataDepGraph, long rgnNum,
                int16_t sigHashSize, LB_ALG lbAlg, SchedPriorities hurstcPrirts,
                SchedPriorities enumPrirts, bool vrfySched, Pruning prune,
                bool enblStallEnum, int spillCostFactor, 
                SPILL_COST_FUNCTION spillCostFunc, bool chkSpillCostSum, 
                bool chkCnflcts, bool fixLivein, bool fixLiveout, int maxSpillCost);
    ~BBWithSpill();

    FUNC_RESULT BuildFromFile();

    int CmputCostLwrBound();

    InstCount UpdtOptmlSched(InstSchedule* crntSched,
                             LengthCostEnumerator* enumrtr);
    bool ChkCostFsblty(InstCount trgtLngth, EnumTreeNode* treeNode);
    void SchdulInst(SchedInstruction* inst, InstCount cycleNum,
                    InstCount slotNum, bool trackCnflcts);
    void UnschdulInst(SchedInstruction* inst, InstCount cycleNum,
                      InstCount slotNum, EnumTreeNode* trgtNode);
    void SetSttcLwrBounds(EnumTreeNode* node);
    bool ChkInstLglty(SchedInstruction* inst);
    void InitForSchdulng();
};

} // end namespace opt_sched

#endif
