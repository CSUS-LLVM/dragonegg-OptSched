//===- OptScheduler.cpp - Implement the opt scheduler -===//
// 
// Integrates an alternative scheduler into LLVM which 
// implements a branch and bound scheduling algorithm.
//
//===-------------------------------------------------===//
#include "llvm/CodeGen/OptSched/OptScheduler.h"
#include "llvm/CodeGen/OptSched/generic/config.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/ScheduleDAGInstrs.h"
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
    // print path to input files
    DEBUG(llvm::dbgs() << "\nOptSched: Path to configuration files:\n" <<
      "Machine Model Config =" << MachineModelConfigFile << "\n" <<
      "Schedule Ini         =" << ScheduleIniFile << "\n" <<
      "Hot Functions Ini    =" << HotfunctionsIniFile << "\n";);

    // Load config files for the OptScheduler
    loadOptSchedConfig();
  }

  void OptScheduler::schedule() {
    if(optSchedEnabled) {
      DEBUG(llvm::dbgs() << "********** Opt Scheduling **********\n");
      defaultScheduler();
    }
    else
      defaultScheduler();
  }

  // call the default "Generic Scheduler" on a region
  void OptScheduler::defaultScheduler() {
    llvm::ScheduleDAGMILive::schedule();
  }
  
  void OptScheduler::loadOptSchedConfig() {
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
  }
} // namespace opt_sched
