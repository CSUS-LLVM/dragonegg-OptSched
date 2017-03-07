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

namespace opt_sched {
  class OptScheduler; 

  // derive from the default scheduler so it is easy to fallback to it
  // when it is needed
  class OptScheduler : public llvm::ScheduleDAGMILive {
    private:
      // Current machine scheduler context
      llvm::MachineSchedContext *context;
      // Wrapper object for converting LLVM information about target machine
      // into the OptSched machine model
      LLVMMachineModel model;
    public:
      OptScheduler(llvm::MachineSchedContext* C);
      void defaultScheduler();
      void schedule() override;
  };

} // namespace opt_sched
#endif
