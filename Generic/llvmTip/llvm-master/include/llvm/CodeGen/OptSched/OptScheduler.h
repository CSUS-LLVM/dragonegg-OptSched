//===- OptScheduler.h - An Alternative MachineInstr scheduler --*- C++ -*-===//
//
// Integrates an alternative scheduler into LLVM which implements a
// branch and bound scheduling algorithm.
//
//===---------------------------------------------------------------------===//

#ifndef OPTSCHED_OPTSCHEDULER_H
#define OPTSCHED_OPTSCHEDULER_H

#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/OptSched/OptSchedMachineWrapper.h"
#include "llvm/CodeGen/OptSched/generic/config.h"
#include "llvm/CodeGen/OptSched/basic/data_dep.h"

namespace opt_sched {
  class OptScheduler; 

  // derive from the default scheduler so it is easy to fallback to it
  // when it is needed. This object is created for each function the 
  // Machine Schduler schedules
  class OptScheduler : public llvm::ScheduleDAGMILive {
    private:
      // Current machine scheduler context
      llvm::MachineSchedContext* context;
      // Wrapper object for converting LLVM information about target machine
      // into the OptSched machine model
      LLVMMachineModel model;
      // Configuration object for the OptScheduler which loads settings from
      // the scheduler initialization file "ScheduleIniFile"
      Config schedIni;
      // A list of functions that are indicated as candidates for the
      // OptScheduler
      Config hotFunctions;
      // Flag indicating whether the optScheduler should be enabled for this function
      bool optSchedEnabled;
      // Precision of latency info
      LATENCY_PRECISION latencyPrecision;
      // The maximum DAG size to be scheduled using precise latency information.
      int maxDagSizeForLatencyPrecision;
      // Treat data dependencies of type ORDER as data dependencies
      bool treatOrderDepsAsDataDeps;

      // Load config files for the OptScheduler than check if 
      // OptScheduler is enabled in config file or if we only 
      // want to run the expensive scheduler on "Hot Functions" only
      void loadOptSchedConfig();
    public:
      OptScheduler(llvm::MachineSchedContext* C);
      ~OptScheduler() {}
      // The fallback LLVM scheduler
      void defaultScheduler();
      // Schedule the current region using the OptScheduler
      void schedule() override;
  };

} // namespace opt_sched
#endif
