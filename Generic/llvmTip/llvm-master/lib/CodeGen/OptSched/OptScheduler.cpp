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
#include <iostream>

#define DEBUG_TYPE "optsched"

namespace opt_sched {
  OptScheduler::OptScheduler(llvm::MachineSchedContext* C)
    : llvm::ScheduleDAGInstrs(*C->MF, C->MLI, RemoveKillFlags) {}

  void OptScheduler::schedule() {
    return;
  }
} // namespace opt_sched
