//===- ScheduleDAGOptSched.cpp - Implement the opt scheduler for isel DAG -===//
//
// A wrapper to allow LLVM to access the Opt scheduler.
//
//===----------------------------------------------------------------------===//

// A hack to follow LLVM's build status.
#ifndef NDEBUG
  #define IS_DEBUG
#endif

#define DEBUG_TYPE "pre-RA-sched"
#include "ScheduleDAGSDNodes.h"
#include "llvm/Function.h"
#include "llvm/CodeGen/ScheduleHazardRecognizer.h"
#include "llvm/CodeGen/SchedulerRegistry.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/OptSched/generic/config.h"
#include "llvm/CodeGen/OptSched/sched_region/sched_region.h"
#include "llvm/CodeGen/OptSched/spill/bb_spill.h"
#include "llvm/CodeGen/OptSched/OptSchedMachineWrapper.h"
#include "llvm/CodeGen/OptSched/OptSchedDAGWrapper.h"

using namespace llvm;
using namespace opt_sched;

static RegisterScheduler
  optDAGScheduler("opt", "Opt scheduler", createOptDAGScheduler);

// TODO(max99x): Make this via command line.
//const std::string INPUT_PATH = "/home/max/sched/trunk/input";
//const std::string INPUT_PATH = ".";
const std::string INPUT_PATH = "/home/nano/llvm-3.3/input";

// Ugly hacks to print spill info and provide the options
// to bypass MachineCSE and/or MachineLICM
extern bool gIsHotFunc;
extern int gHotFuncNum;
extern bool gBypassMachineCSE;
extern bool gBypassMachineLICM;

// An enum specifying the user's choice of which LLVM scheduler
// to use. This allows the user to override LLVM's default scheduler
enum LLVM_SCHEDULER {
  LS_ILP,
  LS_BURR,
  LS_FAST,
  LS_DEFAULT
};
LLVM_SCHEDULER ParseLLVMSched(const string& name);

namespace {

//===----------------------------------------------------------------------===//
/// ScheduleDAGOpt - A bridge to the Opt scheduler.
///
class ScheduleDAGOpt : public ScheduleDAGSDNodes {
public:
  ScheduleDAGOpt(SelectionDAGISel *IS, CodeGenOpt::Level optLevel)
      : ScheduleDAGSDNodes(*IS->MF), ISel(IS), model(IS, INPUT_PATH + "/machine_model.cfg") {
    config.Load(INPUT_PATH + "/sched.ini");
    bool ilp = IS->getTargetLowering().getSchedulingPreference() == Sched::ILP;
    listScheduler = ilp ? createILPListDAGScheduler(IS, optLevel)
                        : createBURRListDAGScheduler(IS, optLevel);
  }

  ~ScheduleDAGOpt() {
    if (listScheduler) delete listScheduler;
  }

  void Schedule();

private:
  ScheduleDAGSDNodes* listScheduler;
  SelectionDAGISel *ISel;
  LLVMMachineModel model;
  Config config;

  void ScheduleNode(SUnit *SU, unsigned CurCycle);
  SchedRegion* CreateRegion(DataDepGraph& dag);
  void RunFallbackScheduler();

  LISTSCHED_HEURISTIC ParseHeuristic(const string& name) const;
  LB_ALG ParseLBAlg(const string& name) const;
  SPILL_COST_FUNCTION ParseSpillCostFunc(const string& name) const;
  LATENCY_PRECISION ParseLatencyPrecision(const string& name) const;
};

}  // end anonymous namespace

/// Schedule - Schedule the DAG using opt scheduling.
void ScheduleDAGOpt::Schedule() {
  DEBUG(dbgs() << "********** Opt Scheduling **********\n");
  Logger::Info("\n********** Starting Opt Scheduling **********");

  if (config.GetString("HEURISTIC") == "FSO") {
    RunFallbackScheduler();
    // Copy the sequence order.
    for (size_t i = 0; i < listScheduler->Sequence.size(); ++i) {
      // We're using NodeQueueId to record the original order as it's unused.
      SUnits[listScheduler->Sequence[i]->NodeNum].NodeQueueId = i;
    }
  } else {
    // Build the LLVM scheduling graph.
    BuildSchedGraph(NULL);
  }

  // Ignore empty DAGs. These actually happen.
  if (SUnits.empty()) return;

  // Write out the scheduling units.
  #ifdef IS_DEBUG_LLVM_SDOPT
    for (std::vector<SUnit>::iterator it = SUnits.begin();
         it != SUnits.end();
         it++) {
      it->dumpAll(this);
    }
  #endif

  LATENCY_PRECISION ltncyPrcsn = ParseLatencyPrecision(config.GetString("LATENCY_PRECISION"));
  int maxDagSizeForPrcisLtncy = config.GetInt("MAX_DAG_SIZE_FOR_PRECISE_LATENCY", 10000);

  // Convert LLVM DAG to opt_sched::DataDepGraph.
  LLVMDataDepGraph dag(ISel, this, &model, ltncyPrcsn, config.GetBool("TREAT_ORDER_DEPS_AS_DATA_DEPS", false), maxDagSizeForPrcisLtncy);
  SchedRegion* region = CreateRegion(dag);
  region->BuildFromFile();

  // Schedule.
  bool isEasy;
  InstCount normBestCost = 0;
  InstCount bestSchedLngth = 0;
  InstCount normHurstcCost = 0;
  InstCount hurstcSchedLngth = 0;
  InstSchedule* sched = NULL;
  
  int rgnTimeout = config.GetInt("REGION_TIMEOUT", INVALID_VALUE);
  int lngthTimeout = config.GetInt("LENGTH_TIMEOUT", INVALID_VALUE);
  if(config.GetString("TIMEOUT_PER") == "INSTR") {
    rgnTimeout *= dag.GetInstCnt();
    lngthTimeout *= dag.GetInstCnt();
  }
  Logger::Info("Region time limit : %d. Length time limit : %d ", rgnTimeout, lngthTimeout);

  int minDagSize = config.GetInt("MIN_DAG_SIZE", 0);
  int maxDagSize = config.GetInt("MAX_DAG_SIZE", 10000);

  FUNC_RESULT rslt ;

  if(dag.GetInstCnt() < minDagSize || dag.GetInstCnt() > maxDagSize) {
     rslt = RES_FAIL;
     Logger::Error("Dag skipped due to out-of-range size. DAG size = %d, valid range is [%d, %d]", dag.GetInstCnt(), minDagSize, maxDagSize);
  } else {
  rslt = region->FindOptimalSchedule(
      config.GetBool("USE_FILE_BOUNDS", true),
      rgnTimeout,
      lngthTimeout,
      isEasy,
      normBestCost,
      bestSchedLngth,
      normHurstcCost,
      hurstcSchedLngth,
      sched);
  }

  if(!(rslt == RES_SUCCESS || rslt == RES_TIMEOUT) || sched == NULL || 
      (rslt == RES_TIMEOUT && normBestCost == normHurstcCost && config.GetString("HEURISTIC") == "FSO")) {  
    Logger::Error("OptSched run failed: rslt=%d, sched=%p. Falling back.",
                  rslt, (void*)sched);
    // Run the fallback LLVM scheduler if we haven't done so already.
    if (config.GetString("HEURISTIC") != "FSO") RunFallbackScheduler();
    Sequence = listScheduler->Sequence;
  } else {
    Logger::Info("OptSched succeeded.");
    // Convert back to LLVM.
    InstCount cycle, slot;
    for (InstCount i = sched->GetFrstInst(cycle, slot);
         i != INVALID_VALUE;
         i = sched->GetNxtInst(cycle, slot)) {
      if (i == SCHD_STALL) {
        ScheduleNode(NULL, cycle);
      } else {
        SUnit* unit = dag.GetSUnit(i);
        // TODO(max99x): Prettify this into something like isRealInstruction().
        assert((size_t)i == SUnits.size() || (size_t)i == SUnits.size() + 1 ||
               unit);
        if (unit) ScheduleNode(unit, cycle);
      }
    }
  }

  // Cleanup.
  delete region;
  delete sched;
}

/// ScheduleNode - Add the node to the schedule.
void ScheduleDAGOpt::ScheduleNode(SUnit *SU, unsigned CurCycle) {
  DEBUG(dbgs() << "*** Scheduling [" << CurCycle << "]: ");
  Sequence.push_back(SU);
  if (SU) {
    DEBUG(SU->dump(this));
  } else {
    DEBUG(dbgs() << "STALL\n");
  }
}

void ScheduleDAGOpt::RunFallbackScheduler() {
  // Run the LLVM list scheduler.
  listScheduler->DAG = DAG;
  listScheduler->BB = BB;
  listScheduler->InsertPos = InsertPos;

  listScheduler->SUnits.clear();
  listScheduler->Sequence.clear();
  listScheduler->EntrySU = SUnit();
  listScheduler->ExitSU = SUnit();

  listScheduler->Schedule();

  assert(listScheduler->SUnits.size() == listScheduler->Sequence.size() && 
         "Not all SUnits scheduled!");

  // Copy the SUnits created by the list scheduler.
  for (size_t i = 0; i < listScheduler->SUnits.size(); ++i) {
    SUnits.push_back(listScheduler->SUnits[i]);
  }
}

SchedRegion* ScheduleDAGOpt::CreateRegion(DataDepGraph& dag) {
  Pruning prune;
  prune.rlxd = config.GetBool("APPLY_RELAXED_PRUNING", true);
  prune.nodeSup = config.GetBool("APPLY_NODE_SUPERIORITY", true);
  prune.histDom = config.GetBool("APPLY_HISTORY_DOMINATION", true);
  prune.spillCost = config.GetBool("APPLY_SPILL_COST_PRUNING", true);
  return new BBWithSpill(&model,
                         &dag,
                         0,
                         (int16_t)config.GetInt("HIST_TABLE_HASH_BITS"),
                         ParseLBAlg(config.GetString("LB_ALG", "LC")),
                         ParseHeuristic(config.GetString("HEURISTIC")),
                         ParseHeuristic(config.GetString("ENUM_HEURISTIC")),
                         config.GetBool("VERIFY_SCHEDULE", false),
                         prune,
                         config.GetBool("ENUMERATE_STALLS", true),
                         (int)config.GetInt("SPILL_COST_FACTOR"),
                         ParseSpillCostFunc(config.GetString("SPILL_COST_FUNCTION")),
                         config.GetBool("CHECK_SPILL_COST_SUM", true),
                         config.GetBool("CHECK_CONFLICTS", true),
                         config.GetBool("FIX_LIVEIN", false),
                         config.GetBool("FIX_LIVEOUT", false),
                         (int)config.GetInt("MAX_SPILL_COST"));
}

LISTSCHED_HEURISTIC ScheduleDAGOpt::ParseHeuristic(const string& name) const {
  if (name == "CP") {
    return LSH_CP;
  } else if (name == "FSO") {
    return LSH_FSO;
  } else {
    Logger::Error("Unrecognized heuristic. Defaulted to CP.");
    return LSH_CP;
  }
}

LB_ALG ScheduleDAGOpt::ParseLBAlg(const string& name) const {
  if (name == "RJ") {
    return LBA_RJ;
  } else if (name == "LC") {
    return LBA_LC;
  } else {
    Logger::Error("Unrecognized lower bound technique. Defaulted to Rim-Jain.");
    return LBA_RJ;
  }
}

SPILL_COST_FUNCTION ScheduleDAGOpt::ParseSpillCostFunc(const string& name) const {

  if (name == "PEAK") {
    return SCF_PEAK;
  } else if (name == "PEAK_PER_TYPE") {
    return SCF_PEAK_PER_TYPE;
  } else if (name == "SUM") {
    return SCF_SUM;
  } else if (name == "PEAK_PLUS_AVG") {
    return SCF_PEAK_PLUS_AVG;
  } else {
    Logger::Error("Unrecognized spill cost function. Defaulted to PEAK.");
    return SCF_PEAK;
  }
}

LLVM_SCHEDULER ParseLLVMSched(const string& name) {
  if (name == "ILP") {
    return LS_ILP;
  } else if (name == "BURR") {
    return LS_BURR;
  } else if (name == "FAST") {
    return LS_FAST;
  } else if (name == "DEFAULT") {
    return LS_DEFAULT;
  } else {
    Logger::Error("Unrecognized LLVM scheduler. Defaulted to LLVM's default scheduler for this target.");
    return LS_DEFAULT;
  }
}

LATENCY_PRECISION ScheduleDAGOpt::ParseLatencyPrecision(const string& name) const {
  if (name == "PRECISE") {
    return LTP_PRECISE;
  } else if (name == "ROUGH") {
    return LTP_ROUGH;
  } else if (name == "UNITY") {
    return LTP_UNITY;
  } else {
    Logger::Error("Unrecognized latency precision. Defaulted to PRECISE.");
    return LTP_PRECISE;
  }
}

//===----------------------------------------------------------------------===//
//                         Public Constructor Functions
//===----------------------------------------------------------------------===//

/// createOptDAGScheduler - This creates an Opt scheduler.
ScheduleDAGSDNodes *
llvm::createOptDAGScheduler(SelectionDAGISel *IS, CodeGenOpt::Level optLevel) {
  // TODO(max99x): Make sure the main config is loaded once, not twice.
  Config config;
  config.Load(INPUT_PATH + "/sched.ini");
  std::string option = config.GetString("USE_OPT_SCHED");
  std::string fxnName = IS->MF->getFunction()->getNameStr();
  LLVM_SCHEDULER llvmSched = ParseLLVMSched(config.GetString("LLVM_SCHEDULER"));
/*  static std::string prevFuncName = "*";

  if (fxnName != prevFuncName)
    Logger::Info("********** Processing function %s", fxnName.c_str());
  prevFuncName = fxnName;*/ 

  Config hotFxns;
  hotFxns.Load(INPUT_PATH + "/hotfuncs.ini");
  gIsHotFunc = hotFxns.GetBool(fxnName, false);

  bool useOpt;
  if (option == "YES") {
    useOpt = true;
  } else if (option == "NO") {
    useOpt = false;
  } else if (option == "HOT_ONLY") {
    useOpt = gIsHotFunc;
  } else {
    Logger::Error("Unknown value for USE_OPT_SCHED: %s. Assuming NO.",
                  option.c_str());
    useOpt = false;
  } 

  gBypassMachineCSE = false;
  gBypassMachineLICM = false;
  if(gIsHotFunc) {
    gBypassMachineCSE = config.GetBool("BYPASS_MACHINE_CSE", false);
    gBypassMachineLICM = config.GetBool("BYPASS_MACHINE_LICM", false);
  }

  if(useOpt) {
    return new ScheduleDAGOpt(IS, optLevel);
  } else {
    switch(llvmSched) {
      case LS_ILP:
        return createILPListDAGScheduler(IS, optLevel);
      case LS_BURR:
        return createBURRListDAGScheduler(IS, optLevel);
      case LS_FAST:
        return createFastDAGScheduler(IS, optLevel);
      case LS_DEFAULT:
        if(IS->getTargetLowering().getSchedulingPreference() == Sched::ILP)
          return createILPListDAGScheduler(IS, optLevel);
        else return createBURRListDAGScheduler(IS, optLevel);
    }
  }
}
