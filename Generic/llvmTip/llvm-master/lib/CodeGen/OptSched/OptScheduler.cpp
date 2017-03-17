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
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/ScheduleDAGInstrs.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"

#define DEBUG_TYPE "optsched"

// read path to configuration files from command line
static llvm::cl::opt<std::string> MachineModelConfigFile("optsched-mcfg", llvm::cl::Hidden,
                                  llvm::cl::desc("Path to machine model configuration file"));

static llvm::cl::opt<std::string> ScheduleIniFile("optsched-sini", llvm::cl::Hidden,
                                  llvm::cl::desc("Path to scheduler initialization file"));

static llvm::cl::opt<std::string> HotfunctionsIniFile("optsched-hfini", llvm::cl::Hidden,
                                  llvm::cl::desc("Path to hot functions initialization file"));

namespace opt_sched {
  OptScheduler::OptScheduler(llvm::MachineSchedContext* C)
    : llvm::ScheduleDAGMILive(C, llvm::make_unique<llvm::GenericScheduler>(C)),
      context(C), model(C, MachineModelConfigFile) {
    // Load config files for the OptScheduler
    loadOptSchedConfig();
  }

  void OptScheduler::schedule() {
    if(!optSchedEnabled) {
      defaultScheduler();
      return;
    }

    DEBUG(llvm::dbgs() << "********** Opt Scheduling **********\n");

		// build DAG
		buildSchedGraph(AA);
    // Ignore empty DAGs
    if(SUnits.empty())
      return;
		//TODO(austin) remove print out dag
		for (std::vector<llvm::SUnit>::iterator it = SUnits.begin();
    	   it != SUnits.end(); it++) {
      it->dumpAll(this);
    }

    // convert build dag
    LLVMDataDepGraph dag(context, this, &model, latencyPrecision,
                         treatOrderDepsAsDataDeps, maxDagSizeForLatencyPrecision);
  }

  // call the default "Generic Scheduler" on a region
  void OptScheduler::defaultScheduler() {
    llvm::ScheduleDAGMILive::schedule();
  }
  
  void OptScheduler::loadOptSchedConfig() {
		// print path to input files
    DEBUG(llvm::dbgs() << "\nOptSched: Path to configuration files:\n" <<
      "Machine Model Config =" << MachineModelConfigFile << "\n" <<
      "Schedule Ini         =" << ScheduleIniFile << "\n" <<
      "Hot Functions Ini    =" << HotfunctionsIniFile << "\n";);
    // load OptSched ini file
    schedIni.Load(ScheduleIniFile);
    // load hot functions ini file
    hotFunctions.Load(HotfunctionsIniFile);

    // check scheduler ini file to see if optsched is enabled
    std::string optSchedOption = schedIni.GetString("USE_OPT_SCHED");
    if(optSchedOption == "YES") {
      optSchedEnabled = true;
    }
    else if(optSchedOption == "HOT_ONLY") {
      // get the name of the function this scheduler was created for
      std::string functionName = context->MF->getFunction()->getName();
      // check the list of hot functions for the name of the current function
      optSchedEnabled = hotFunctions.GetBool(functionName, false); 
    }
    else if (optSchedOption == "NO") {
      optSchedEnabled = false;
    }
    else {
      DEBUG(llvm::dbgs() << "Invalid value for USE_OPT_SCHED" << 
           optSchedOption << "Assuming NO.\n");
      optSchedEnabled = false;
    }

    // Set latency precision setting from config file
    std::string lpName = schedIni.GetString("LATENCY_PRECISION");
		if (lpName == "PRECISE") {
      latencyPrecision = LTP_PRECISE;
    } else if (lpName == "ROUGH") {
      latencyPrecision = LTP_ROUGH;
    } else if (lpName == "UNITY") {
      latencyPrecision = LTP_UNITY;
    } else {
      Logger::Error("Unrecognized latency precision. Defaulted to PRECISE.");
      latencyPrecision = LTP_PRECISE;
    }
    
    maxDagSizeForLatencyPrecision = schedIni.GetInt("MAX_DAG_SIZE_FOR_PRECISE_LATENCY",
                                                    10000);
    treatOrderDepsAsDataDeps = schedIni.GetBool("TREAT_ORDER_DEPS_AS_DATA_DEPS", false);
  }
} // namespace opt_sched
