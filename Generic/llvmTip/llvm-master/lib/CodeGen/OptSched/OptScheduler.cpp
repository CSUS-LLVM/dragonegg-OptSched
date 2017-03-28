//===- OptScheduler.cpp - Implement the opt scheduler -===//
// 
// Integrates an alternative scheduler into LLVM which 
// implements a branch and bound scheduling algorithm.
//
//===-------------------------------------------------===//
#include "llvm/CodeGen/OptSched/OptScheduler.h"
#include "llvm/CodeGen/OptSched/generic/config.h"
#include "llvm/CodeGen/OptSched/basic/data_dep.h"
#include "llvm/CodeGen/OptSched/OptSchedDagWrapper.h"
#include "llvm/CodeGen/OptSched/sched_region/sched_region.h"
#include "llvm/CodeGen/OptSched/spill/bb_spill.h"
#include "llvm/CodeGen/OptSched/generic/utilities.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/ScheduleDAGInstrs.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include <chrono>

#define DEBUG_TYPE "optsched"

// read path to configuration files from command line
static llvm::cl::opt<std::string> MachineModelConfigFile("optsched-mcfg", llvm::cl::Hidden,
                                  llvm::cl::desc("Path to machine model configuration file"));

static llvm::cl::opt<std::string> ScheduleIniFile("optsched-sini", llvm::cl::Hidden,
                                  llvm::cl::desc("Path to scheduler initialization file"));

static llvm::cl::opt<std::string> HotfunctionsIniFile("optsched-hfini", llvm::cl::Hidden,
                                  llvm::cl::desc("Path to hot functions initialization file"));

// If this iterator is a debug value, increment until reaching the End or a
// non-debug instruction. static method from llvm/CodeGen/MachineScheduler.cpp
static llvm::MachineBasicBlock::iterator
nextIfDebug(llvm::MachineBasicBlock::iterator I,
            llvm::MachineBasicBlock::const_iterator End) {
	for(; I != End; ++I) {
    if (!I->isDebugValue())
      break;
  }
  return I;
}

using namespace std::chrono;
milliseconds ms = duration_cast< milliseconds >(
    system_clock::now().time_since_epoch()
);
std::chrono::milliseconds opt_sched::ScheduleDAGOptSched::startTime = ms;

namespace opt_sched {
  ScheduleDAGOptSched::ScheduleDAGOptSched(llvm::MachineSchedContext* C)
    : llvm::ScheduleDAGMILive(C, llvm::make_unique<llvm::GenericScheduler>(C)),
      context(C), model(MachineModelConfigFile) {
    // valid heuristic names
		std::strcpy(hurstcNames[(int)LSH_CP], "CP");
    std::strcpy(hurstcNames[(int)LSH_LUC], "LUC");
    std::strcpy(hurstcNames[(int)LSH_UC], "UC");
    std::strcpy(hurstcNames[(int)LSH_NID], "NID");
    std::strcpy(hurstcNames[(int)LSH_CPR], "CPR");
    std::strcpy(hurstcNames[(int)LSH_ISO], "ISO");
    std::strcpy(hurstcNames[(int)LSH_SC], "SC");
    std::strcpy(hurstcNames[(int)LSH_LS], "LS");
    // Convert target information into machine model
    model.convertMachineModel(this);
    // Load config files for the OptScheduler
    loadOptSchedConfig();
  }

  void ScheduleDAGOptSched::schedule() {
    if(!optSchedEnabled) {
      defaultScheduler();
      return;
    }

    DEBUG(llvm::dbgs() << "********** Opt Scheduling **********\n");

    std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
      std::chrono::system_clock::now().time_since_epoch()
  	);
  	Milliseconds startTime = ms.count();

		// build DAG
		buildSchedGraph(AA);
    // Ignore empty DAGs
    if(SUnits.empty())
      return;

    // convert dag
    LLVMDataDepGraph dag(context, this, &model, latencyPrecision,
                         treatOrderDepsAsDataDeps, maxDagSizeForLatencyPrecision);
		// create region
	  SchedRegion* region = new BBWithSpill(&model,
                                     &dag,
                                     0,
                                     histTableHashBits,
                                     lowerBoundAlgorithm,
                                     heuristicPriorities,
                                     enumPriorities,
                                     verifySchedule,
                                     prune,
                                     enumerateStalls,
                                     spillCostFactor,
                                     spillCostFunction,
                                     checkSpillCostSum,
                                     checkConflicts,
                                     fixLiveIn,
                                     fixLiveOut,
                                     maxSpillCost);
    region->BuildFromFile();

    // Schedule
		bool isEasy;
    InstCount normBestCost = 0;
    InstCount bestSchedLngth = 0;
    InstCount normHurstcCost = 0;
    InstCount hurstcSchedLngth = 0;
    InstSchedule* sched = NULL;
    FUNC_RESULT rslt;
    if(isTimeoutPerInstruction) {
      regionTimeout *= dag.GetInstCnt();
      lengthTimeout *= dag.GetInstCnt();
    }

    if(dag.GetInstCnt() < minDagSize || dag.GetInstCnt() > maxDagSize) {
      rslt = RES_FAIL;
		Logger::Error("Dag skipped due to out-of-range size. DAG size = %d, \
									valid range is [%d, %d]", dag.GetInstCnt(), minDagSize, maxDagSize);
    } else {
      rslt = region->FindOptimalSchedule(useFileBounds,
                                         regionTimeout,
                                         lengthTimeout,
                                         isEasy,
                                         normBestCost,
                                         bestSchedLngth,
                                         normHurstcCost,
                                         hurstcSchedLngth,
                                         sched);
    }

    
    if((!(rslt == RES_SUCCESS || rslt == RES_TIMEOUT) || sched == NULL)) {
			Logger::Error("OptSched run failed: rslt=%d, sched=%p. Falling back.",
                  rslt, (void*)sched);
			//TODO(austin) run fallback scheduler
    } else {
	    Logger::Info("OptSched succeeded.");	
      // Convert back to LLVM.
      // Advance past initial DebugValues.
      CurrentTop = nextIfDebug(RegionBegin, RegionEnd);
      CurrentBottom = RegionEnd;
      InstCount cycle, slot;
      for (InstCount i = sched->GetFrstInst(cycle, slot);
           i != INVALID_VALUE;
           i = sched->GetNxtInst(cycle, slot)) {
        if (i == SCHD_STALL) {
          ScheduleNode(NULL, cycle);
        } else {
          llvm::SUnit* unit = dag.GetSUnit(i);
          if (unit && unit->isInstr()) ScheduleNode(unit, cycle);
        }
      }
		}
    
    delete region;
  }

	void ScheduleDAGOptSched::ScheduleNode(llvm::SUnit *SU, unsigned CurCycle) {
		#ifdef IS_DEBUG_CONVERT_LLVM
  	Logger::Info("*** Scheduling [%lu]: ", CurCycle);
		#endif
  	if (SU) {
      llvm::MachineInstr* instr = SU->getInstr();
      if (CurrentTop == NULL){
        Logger::Error("Currenttop is NULL");
        return;
      }
      if (&*CurrentTop == instr)
        CurrentTop = nextIfDebug(++CurrentTop, CurrentBottom);
      else
        moveInstruction(instr, CurrentTop);
  	} else {
			#ifdef IS_DEBUG_CONVERT_LLVM
    	Logger::Info("Stall");
			#endif
  	}
}

  // call the default "Generic Scheduler" on a region
  void ScheduleDAGOptSched::defaultScheduler() {
    llvm::ScheduleDAGMILive::schedule();
  }
  
  void ScheduleDAGOptSched::loadOptSchedConfig() {
		// print path to input files
    DEBUG(llvm::dbgs() << "\nOptSched: Path to configuration files:\n" <<
      "Machine Model Config =" << MachineModelConfigFile << "\n" <<
      "Schedule Ini         =" << ScheduleIniFile << "\n" <<
      "Hot Functions Ini    =" << HotfunctionsIniFile << "\n";);
    // load OptSched ini file
    schedIni.Load(ScheduleIniFile);
    // load hot functions ini file
    hotFunctions.Load(HotfunctionsIniFile);

    // setup OptScheduler configuration options 
		optSchedEnabled = isOptSchedEnabled();
    latencyPrecision = fetchLatencyPrecision();
    maxDagSizeForLatencyPrecision = schedIni.GetInt("MAX_DAG_SIZE_FOR_PRECISE_LATENCY");
    treatOrderDepsAsDataDeps = schedIni.GetBool("TREAT_ORDER_DEPS_AS_DATA_DEPS");

    // setup pruning
		prune.rlxd = schedIni.GetBool("APPLY_RELAXED_PRUNING");
  	prune.nodeSup = schedIni.GetBool("APPLY_NODE_SUPERIORITY");
  	prune.histDom = schedIni.GetBool("APPLY_HISTORY_DOMINATION");
  	prune.spillCost = schedIni.GetBool("APPLY_SPILL_COST_PRUNING");
	
		histTableHashBits = static_cast<int16_t>(schedIni.GetInt("HIST_TABLE_HASH_BITS"));
		verifySchedule = schedIni.GetBool("VERIFY_SCHEDULE");
		enumerateStalls = schedIni.GetBool("ENUMERATE_STALLS");
		spillCostFactor = schedIni.GetInt("SPILL_COST_FACTOR");
		checkSpillCostSum = schedIni.GetBool("CHECK_SPILL_COST_SUM");
		checkConflicts = schedIni.GetBool("CHECK_CONFLICTS");
		fixLiveIn = schedIni.GetBool("FIX_LIVEIN");
		fixLiveOut = schedIni.GetBool("FIX_LIVEOUT");
		maxSpillCost = schedIni.GetInt("MAX_SPILL_COST");
		lowerBoundAlgorithm = parseLowerBoundAlgorithm();
		heuristicPriorities = parseHeuristic(schedIni.GetString("HEURISTIC"));
		enumPriorities = parseHeuristic(schedIni.GetString("ENUM_HEURISTIC"));
	  spillCostFunction = parseSpillCostFunc();
    regionTimeout = schedIni.GetInt("REGION_TIMEOUT");
    lengthTimeout = schedIni.GetInt("LENGTH_TIMEOUT");
    if(schedIni.GetString("TIMEOUT_PER") == "INSTR")
      isTimeoutPerInstruction = true;
    minDagSize = schedIni.GetInt("MIN_DAG_SIZE");
    maxDagSize = schedIni.GetInt("MAX_DAG_SIZE");
    useFileBounds = schedIni.GetBool("USE_FILE_BOUNDS");
  }

	bool ScheduleDAGOptSched::isOptSchedEnabled() const {
		// check scheduler ini file to see if optsched is enabled
    std::string optSchedOption = schedIni.GetString("USE_OPT_SCHED");
    if(optSchedOption == "YES") {
			return true;
    }
    else if(optSchedOption == "HOT_ONLY") {
      // get the name of the function this scheduler was created for
      std::string functionName = context->MF->getFunction()->getName();
      // check the list of hot functions for the name of the current function
      return hotFunctions.GetBool(functionName, false);
    }
    else if (optSchedOption == "NO") {
    	return false;
    }
    else {
      DEBUG(llvm::dbgs() << "Invalid value for USE_OPT_SCHED" <<
           optSchedOption << "Assuming NO.\n");
      return false;
    }
	}

  LATENCY_PRECISION ScheduleDAGOptSched::fetchLatencyPrecision() const {
		std::string lpName = schedIni.GetString("LATENCY_PRECISION");
    if (lpName == "PRECISE") {
      return LTP_PRECISE;
    } else if (lpName == "ROUGH") {
       return LTP_ROUGH;
    } else if (lpName == "UNITY") {
      return LTP_UNITY;
    } else {
      Logger::Error("Unrecognized latency precision. Defaulted to PRECISE.");
      return LTP_PRECISE;
    }	
  }

	LB_ALG ScheduleDAGOptSched::parseLowerBoundAlgorithm() const {
		std::string LBalg = schedIni.GetString("LB_ALG");
  	if (LBalg == "RJ") {
    	return LBA_RJ;
  	} else if (LBalg == "LC") {
    	return LBA_LC;
  	} else {
    	Logger::Error("Unrecognized lower bound technique. Defaulted to Rim-Jain.");
    	return LBA_RJ;
  	}
	}

	SchedPriorities ScheduleDAGOptSched::parseHeuristic(const std::string &str) const {
		SchedPriorities prirts;
  	int len = str.length();
  	char word[HEUR_NAME_MAX_SIZE];
  	int wIndx = 0;
  	prirts.cnt = 0;
  	prirts.isDynmc = false;
  	int i, j;
 
  	for(i=0; i<= len; i++) {
    	char ch = str.c_str()[i];
    	if (ch  == '_' || ch == 0) { // end of word
      	word[wIndx] = 0;     
      	for (j=0; j<HEUR_NAME_CNT; j++) {
        	if (strcmp(word, hurstcNames[j]) == 0) {
          	prirts.vctr[prirts.cnt] = (LISTSCHED_HEURISTIC) j;
          	if ((LISTSCHED_HEURISTIC)j == LSH_LUC) prirts.isDynmc = true;
          	break;
       		}// end if
      	}// end for j
      	if (j == HEUR_NAME_CNT) {
        	Logger::Error("Unrecognized heuristic %s. Defaulted to CP.", word);
        	prirts.vctr[prirts.cnt] = LSH_CP;
      	}
      	prirts.cnt++;
      	wIndx = 0;
    	}
    	else {
      	word[wIndx] = ch;
      	wIndx++;
    	}// end else
  	}// end for i
		return prirts;
	}

  SPILL_COST_FUNCTION ScheduleDAGOptSched::parseSpillCostFunc() const {
    std::string name = schedIni.GetString("SPILL_COST_FUNCTION");
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
} // namespace opt_sched
