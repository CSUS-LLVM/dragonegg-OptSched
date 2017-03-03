//===- OptScheduler.h - An Alternative MachineInstr scheduler --*- C++ -*-===//
//
// Integrates an alternative scheduler into LLVM which implements a
// branch and bound scheduling algorithm.
//
//===---------------------------------------------------------------------===//

#ifndef OPTSCHED_OPTSCHEDULER_H
#define OPTSCHED_OPTSCHEDULER_H

#include "llvm/CodeGen/MachineScheduler.h"

namespace opt_sched {
  class OptScheduler; 

  // derive from the default scheduler so it is easy to fallback to it
  // when needed
  class OptScheduler : public llvm::ScheduleDAGMILive {
    private:
      llvm::MachineSchedContext *context;
    public:
      OptScheduler(llvm::MachineSchedContext* C);
      void defaultScheduler();
      void schedule() override;
  };

} // namespace opt_sched
#endif
