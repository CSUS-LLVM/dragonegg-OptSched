#include <algorithm>

#include "llvm/CodeGen/OptSched/sched_region/sched_region.h"
#include "llvm/CodeGen/OptSched/generic/logger.h"
#include "llvm/CodeGen/OptSched/generic/random.h"
#include "llvm/CodeGen/OptSched/generic/utilities.h"
#include "llvm/CodeGen/OptSched/generic/stats.h"
#include "llvm/CodeGen/OptSched/list_sched/list_sched.h"
#include "llvm/CodeGen/OptSched/relaxed/relaxed_sched.h"
#include "llvm/CodeGen/OptSched/spill/bb_spill.h"

namespace opt_sched {

SchedRegion::SchedRegion(MachineModel* machMdl,
                         DataDepGraph* dataDepGraph,
                         long rgnNum,
                         int16_t sigHashSize,
                         LB_ALG lbAlg,
                         SchedPriorities hurstcPrirts,
                         SchedPriorities enumPrirts,
                         bool vrfySched,
                         Pruning prune) {
  machMdl_ = machMdl;
  dataDepGraph_ = dataDepGraph;
  rgnNum_ = rgnNum;
  sigHashSize_ = sigHashSize;
  lbAlg_ = lbAlg;
  hurstcPrirts_ = hurstcPrirts;
  enumPrirts_ = enumPrirts;
  vrfySched_ = vrfySched;
  prune_ = prune;

  bestCost_ = 0;
  bestSchedLngth_ = 0;
  hurstcCost_ = 0;
  hurstcSchedLngth_ = 0;
  enumCrntSched_ = NULL;
  enumBestSched_ = NULL;

  schedLwrBound_ = 0;
  schedUprBound_ = INVALID_VALUE;

  instCnt_ = dataDepGraph_->GetInstCnt();

  needTrnstvClsr_ = false;
}

void SchedRegion::UseFileBounds_() {
  InstCount fileLwrBound, fileUprBound;

  dataDepGraph_->UseFileBounds();
  dataDepGraph_->GetFileSchedBounds(fileLwrBound, fileUprBound);
  assert(fileLwrBound >= schedLwrBound_);
  schedLwrBound_ = fileLwrBound;
}

InstSchedule* SchedRegion::AllocNewSched_() {
  InstSchedule* newSched = new InstSchedule(machMdl_, dataDepGraph_, vrfySched_);
  if (newSched == NULL) Logger::Fatal("Out of memory.");
  return newSched;
}

void SchedRegion::CmputAbslutUprBound_() {
  abslutSchedUprBound_ = dataDepGraph_->GetAbslutSchedUprBound();
}

FUNC_RESULT SchedRegion::FindOptimalSchedule(bool useFileBounds,
                                             Milliseconds rgnTimeout,
                                             Milliseconds lngthTimeout,
                                             bool& isLstOptml,
                                             InstCount& bestCost,
                                             InstCount& bestSchedLngth,
                                             InstCount& hurstcCost,
                                             InstCount& hurstcSchedLngth,
                                             InstSchedule*& bestSched) {
  ListScheduler* lstSchdulr;
  InstSchedule* lstSched = NULL;
  FUNC_RESULT rslt = RES_SUCCESS;
  Milliseconds hurstcTime = 0;
  Milliseconds boundTime = 0;
  Milliseconds enumTime = 0;
  Milliseconds vrfyTime = 0;

  enumCrntSched_ = NULL;
  enumBestSched_ = NULL;
  bestSched = bestSched_ = NULL;

  Logger::Info("---------------------------------------------------------------------------");
  Logger::Info("Processing DAG %s with %d insts and max latency %d.",
                dataDepGraph_->GetDagID(), dataDepGraph_->GetInstCnt(), dataDepGraph_->GetMaxLtncy());

  Stats::problemSize.Record(dataDepGraph_->GetInstCnt());

  if (rgnTimeout > 0) needTrnstvClsr_ = true; 
  rslt = dataDepGraph_->SetupForSchdulng(needTrnstvClsr_);
//  rslt = dataDepGraph_->SetupForSchdulng(true);
  if (rslt != RES_SUCCESS ) {
   Logger::Info("Invalid input DAG");
   return rslt;
  }
  SetupForSchdulng_();
  CmputAbslutUprBound_();
  schedLwrBound_ = dataDepGraph_->GetSchedLwrBound();
  Milliseconds hurstcStart = Utilities::GetProcessorTime();
  lstSched = new InstSchedule(machMdl_, dataDepGraph_, vrfySched_);
  if (lstSched == NULL) Logger::Fatal("Out of memory.");

  lstSchdulr = AllocLstSchdulr_();

  // Step #1: Find the heuristic schedule.
  rslt = lstSchdulr->FindSchedule(lstSched, this);

  if (rslt != RES_SUCCESS) {
    Logger::Info("List scheduling failed");
    delete lstSchdulr;
    delete lstSched;
    return rslt;
  }

  hurstcTime = Utilities::GetProcessorTime() - hurstcStart;
  Stats::heuristicTime.Record(hurstcTime);
if (hurstcTime > 0) Logger::Info("Heuristic_Time %d",hurstcTime);

  Milliseconds boundStart = Utilities::GetProcessorTime();
  hurstcSchedLngth_ = lstSched->GetCrntLngth();
  bestSchedLngth_ = hurstcSchedLngth_;
  assert(bestSchedLngth_ >= schedLwrBound_);
  bestSched = bestSched_ = lstSched;

  // Step #2: Compute the lower bounds and cost upper bound.
  if (rgnTimeout == 0)
    costLwrBound_ = CmputCostLwrBound();
  else
    CmputLwrBounds_(useFileBounds);
  assert(schedLwrBound_ <= lstSched->GetCrntLngth());

  isLstOptml = CmputUprBounds_(lstSched, useFileBounds);
  boundTime = Utilities::GetProcessorTime() - boundStart;
  Stats::boundComputationTime.Record(boundTime);

  FinishHurstc_();

//  #ifdef IS_DEBUG_SOLN_DETAILS_1
    Logger::Info("The list schedule is of length %d and spill cost %d. Tot cost = %d",
                 bestSchedLngth_, lstSched->GetSpillCost(), bestCost_);
//  #endif

  #ifdef IS_DEBUG_PRINT_SCHEDS
    lstSched->Print(Logger::GetLogStream(), "Heuristic");
  #endif
  #ifdef IS_DEBUG_PRINT_BOUNDS
    dataDepGraph_->PrintLwrBounds(DIR_FRWRD, Logger::GetLogStream(),
                                  "CP Lower Bounds");
  #endif

  if (rgnTimeout == 0) isLstOptml = true;

  if (EnableEnum_() == false) {
    delete lstSchdulr;
    return RES_FAIL;
  }

  // Step #3: Find the optimal schedule if the heuristc was not optimal.
  Milliseconds enumStart = Utilities::GetProcessorTime();

 #ifdef IS_DEBUG_BOUNDS
   Logger::Info("Sched LB = %d, Sched UB = %d",schedLwrBound_, schedUprBound_);
 #endif

  if (isLstOptml == false) {
    dataDepGraph_->SetHard(true);
    rslt = Optimize_(enumStart, rgnTimeout, lngthTimeout);
    Milliseconds enumTime = Utilities::GetProcessorTime() - enumStart;

    if (hurstcTime > 0) {
      enumTime /= hurstcTime;
      Stats::enumerationToHeuristicTimeRatio.Record(enumTime);
    }

    if (bestCost_ < hurstcCost_) {
      assert(enumBestSched_ != NULL);
      bestSched = bestSched_ = enumBestSched_;
      #ifdef IS_DEBUG_PRINT_SCHEDS
        enumBestSched_->Print(Logger::GetLogStream(), "Optimal");
      #endif
    }
  } else {
      if (rgnTimeout == 0)
        Logger::Info("Bypassing optimal scheduling due to zero time limit");
      else
        Logger::Info("The list schedule of length %d and cost %d is optimal.",
                      bestSchedLngth_, bestCost_);
  }

  enumTime = Utilities::GetProcessorTime() - enumStart;
  Stats::enumerationTime.Record(enumTime);

  Milliseconds vrfyStart = Utilities::GetProcessorTime();

  if (vrfySched_) {
    bool isValidSchdul = bestSched->Verify(machMdl_, dataDepGraph_);

    if (isValidSchdul == false) {
      Stats::invalidSchedules++;
    }
  }

  vrfyTime = Utilities::GetProcessorTime() - vrfyStart;
  Stats::verificationTime.Record(vrfyTime);

  InstCount finalLwrBound = costLwrBound_;
  InstCount finalUprBound = costLwrBound_ + bestCost_;
  if (rslt == RES_SUCCESS) finalLwrBound = finalUprBound;

  dataDepGraph_->SetFinalBounds(finalLwrBound, finalUprBound);

  FinishOptml_();

  bool tookBest = ChkSchedule_(bestSched, lstSched);
  if (tookBest == false) {
    bestCost_ = hurstcCost_;
    bestSchedLngth_ = hurstcSchedLngth_;
  }

  delete lstSchdulr;
  if (bestSched != lstSched) delete lstSched;
  if (enumBestSched_ != NULL && bestSched != enumBestSched_) delete enumBestSched_;
  if (enumCrntSched_ != NULL) delete enumCrntSched_;

  bestCost = bestCost_;
  bestSchedLngth = bestSchedLngth_;
  hurstcCost = hurstcCost_;
  hurstcSchedLngth = hurstcSchedLngth_;

  return rslt;
}

FUNC_RESULT SchedRegion::Optimize_(Milliseconds startTime,
                                   Milliseconds rgnTimeout,
                                   Milliseconds lngthTimeout) {
  Enumerator* enumrtr;
  FUNC_RESULT rslt = RES_SUCCESS;

  enumCrntSched_ = AllocNewSched_();
  enumBestSched_ = AllocNewSched_();

  InstCount initCost = bestCost_;
  enumrtr = AllocEnumrtr_(lngthTimeout);
  rslt = Enumerate_(startTime, rgnTimeout, lngthTimeout);

  Milliseconds solnTime = Utilities::GetProcessorTime() - startTime;

  #ifdef IS_DEBUG_NODES
    Logger::Info("Examined %lld nodes.", enumrtr->GetNodeCnt());
  #endif
  Stats::nodeCount.Record(enumrtr->GetNodeCnt());
  Stats::solutionTime.Record(solnTime);

  InstCount imprvmnt = initCost - bestCost_;
  if (rslt == RES_SUCCESS) {
    Logger::Info("DAG solved optimally in %lld ms with "
                 "length=%d, spill cost = %d, tot cost = %d, cost imp=%d.",
                 solnTime,
                 bestSchedLngth_,
                 bestSched_->GetSpillCost(),
                 bestCost_,
                 imprvmnt);
    Stats::solvedProblemSize.Record(dataDepGraph_->GetInstCnt());
    Stats::solutionTimeForSolvedProblems.Record(solnTime);
  } else {
    if(rslt == RES_TIMEOUT) {
      Logger::Info("DAG timed out with "
                   "length=%d, spill cost = %d, tot cost = %d, cost imp=%d.",
                   bestSchedLngth_,
                   bestSched_->GetSpillCost(),
                   bestCost_,
                   imprvmnt);

    }
    Stats::unsolvedProblemSize.Record(dataDepGraph_->GetInstCnt());
  }

  return rslt;
}

void SchedRegion::CmputLwrBounds_(bool useFileBounds) {
  RelaxedScheduler* rlxdSchdulr = NULL;
  RelaxedScheduler* rvrsRlxdSchdulr = NULL;
  InstCount rlxdUprBound = dataDepGraph_->GetAbslutSchedUprBound();

  switch (lbAlg_) {
    case LBA_LC:
      rlxdSchdulr = new LC_RelaxedScheduler(
          dataDepGraph_, machMdl_, rlxdUprBound, DIR_FRWRD);
      rvrsRlxdSchdulr = new LC_RelaxedScheduler(
          dataDepGraph_, machMdl_, rlxdUprBound, DIR_BKWRD);
      break;
    case LBA_RJ:
      rlxdSchdulr = new RJ_RelaxedScheduler(
          dataDepGraph_, machMdl_, rlxdUprBound, DIR_FRWRD, RST_STTC);
      rvrsRlxdSchdulr = new RJ_RelaxedScheduler(
          dataDepGraph_, machMdl_, rlxdUprBound, DIR_BKWRD, RST_STTC);
      break;
  }

  if (rlxdSchdulr == NULL || rvrsRlxdSchdulr == NULL) {
    Logger::Fatal("Out of memory.");
  }

  InstCount frwrdLwrBound = 0;
  InstCount bkwrdLwrBound = 0;
  frwrdLwrBound = rlxdSchdulr->FindSchedule();
  bkwrdLwrBound = rvrsRlxdSchdulr->FindSchedule();
  InstCount rlxdLwrBound = std::max(frwrdLwrBound, bkwrdLwrBound);

  assert(rlxdLwrBound >= schedLwrBound_);

  if (rlxdLwrBound > schedLwrBound_) schedLwrBound_ = rlxdLwrBound;

  #ifdef IS_DEBUG_PRINT_BOUNDS
    dataDepGraph_->PrintLwrBounds(DIR_FRWRD, Logger::GetLogStream(),
                                  "Relaxed Forward Lower Bounds");
    dataDepGraph_->PrintLwrBounds(DIR_BKWRD, Logger::GetLogStream(),
                                  "Relaxed Backward Lower Bounds");
  #endif

  if (useFileBounds) UseFileBounds_();

  costLwrBound_ = CmputCostLwrBound();

  delete rlxdSchdulr;
  delete rvrsRlxdSchdulr;
}

bool SchedRegion::CmputUprBounds_(InstSchedule* lstSched, bool useFileBounds) {
  InstCount hurstcExecCost;
  hurstcCost_ = CmputNormCost_(lstSched, CCM_DYNMC, hurstcExecCost, true);
//  hurstcCost_ = CmputNormCost_(lstSched, CCM_STTC, hurstcExecCost, true);
//  hurstcSchedLngth_ = hurstcExecCost + GetCostLwrBound();

  if (useFileBounds) {
    hurstcCost_ = dataDepGraph_->GetFileCostUprBound();
    hurstcCost_ -= GetCostLwrBound();
  }

  bestCost_ = hurstcCost_;
  bestSchedLngth_ = hurstcSchedLngth_;

  if (bestCost_ == 0) {
    // If the heuristic schedule is optimal, we are done!
    schedUprBound_ = lstSched->GetCrntLngth();
    return true;
  } else {
    CmputSchedUprBound_();
    return false;
  }
}

void SchedRegion::HandlEnumrtrRslt_(FUNC_RESULT rslt,
                                    InstCount trgtLngth) {
  switch (rslt) {
    case RES_FAIL:
//    #ifdef IS_DEBUG_ENUM_ITERS
      Logger::Info("No feasible solution of length %d was found.", trgtLngth);
//    #endif
      break;
    case RES_SUCCESS:
    #ifdef IS_DEBUG_ENUM_ITERS
      Logger::Info("Found a feasible solution of length %d.", trgtLngth);
    #endif
      break;
    case RES_TIMEOUT:
//    #ifdef IS_DEBUG_ENUM_ITERS
      Logger::Info("Enumeration timedout at length %d.", trgtLngth);
//    #endif
      break;
    case RES_ERROR:
      Logger::Info("The processing of DAG \"%s\" was terminated with an error.",
                   dataDepGraph_->GetDagID(), rgnNum_);
      break;
    case RES_END:
//    #ifdef IS_DEBUG_ENUM_ITERS
      Logger::Info("Enumeration ended at length %d.", trgtLngth);
//    #endif
      break;
  }
}

} // end namespace opt_sched
