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

  class OptScheduler : public llvm::ScheduleDAGInstrs {
    public:
      OptScheduler(llvm::MachineSchedContext* C);
      void schedule() override;
  };

} // namespace opt_sched
#endif
