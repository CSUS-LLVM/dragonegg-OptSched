//===- OptScheduler.cpp - Implement the opt scheduler -===//
// 
// Integrates an alternative scheduler into LLVM which 
// implements a branch and bound scheduling algorithm.
//
//===-------------------------------------------------===//
#include "llvm/CodeGen/OptSched/OptScheduler.h"
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
      }

  void OptScheduler::schedule() {
    defaultScheduler();
  }

  // call the default "Generic Scheduler" on a region
  void OptScheduler::defaultScheduler() {
    llvm::ScheduleDAGMILive::schedule();
  }
} // namespace opt_sched
