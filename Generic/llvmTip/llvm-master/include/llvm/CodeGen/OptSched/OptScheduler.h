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
#include "llvm/CodeGen/OptSched/OptSchedDagWrapper.h"
#include "llvm/CodeGen/OptSched/sched_region/sched_region.h"
#include "llvm/CodeGen/OptSched/generic/config.h"
#include "llvm/CodeGen/OptSched/basic/data_dep.h"

#define HEUR_NAME_CNT 8
#define HEUR_NAME_MAX_SIZE 10

namespace opt_sched {
  class ScheduleDAGOptSched; 

  // derive from the default scheduler so it is easy to fallback to it
  // when it is needed. This object is created for each function the 
  // Machine Schduler schedules
  class ScheduleDAGOptSched : public llvm::ScheduleDAGMILive {
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
      // Struct for setting the pruning strategy
      Pruning prune;
      // Flag indicating whether the optScheduler should be enabled for this function
      bool optSchedEnabled;
      // Precision of latency info
      LATENCY_PRECISION latencyPrecision;
      // The maximum DAG size to be scheduled using precise latency information.
      int maxDagSizeForLatencyPrecision;
      // Treat data dependencies of type ORDER as data dependencies
      bool treatOrderDepsAsDataDeps;
      // The number of bits in the hash table used in history-based domination.
      int16_t histTableHashBits;
      // Whether to verify that calculated schedules are optimal. Defaults to NO.
      bool verifySchedule;
			// Whether to enumerate schedules containing stalls (no-op instructions).
			// In certain cases, such as having unpipelined instructions, this may
			// result in a better schedule. Defaults to YES
      bool enumerateStalls;
      // The weight of the spill cost in the objective function. This factor
      // defines the importance of spill cost relative to schedule length. A good
      // value for this factor should be found experimentally, but is is expected
      // to be large on architectures with hardware scheduling like x86 (thus
      // making spill cost minimization the primary objective) and smaller on
      // architectures with in-order execution like SPARC (thus making scheduling
      // the primary objective).
      int spillCostFactor;
      // Check spill cost sum at all points in the block for the enumerator's best
      // schedule and the heuristic schedule. If the latter sum is smaller, take
      // the heuristic schedule instead (if the heuristic sched length is not larger).
      // This can happen, when the SPILL_COST_FUNCTION is not set to SUM.
      bool checkSpillCostSum;
      // Check the total number of conflicts among live ranges for the enumerator's best
      // schedule and the heuristic schedule. If the latter is smaller, take
      // the heuristic schedule instead (if the heuristic sched length is not larger).
      // Check conflicts
      bool checkConflicts;
      // Force CopyFromReg instrs to be scheduled before all other instrs in the block
      bool fixLiveIn;
      // Force CopyToReg instrs to be scheduled after all other instrs in the block 
      bool fixLiveOut;
      // The spill cost function to be used.
      SPILL_COST_FUNCTION spillCostFunction;
      // The maximum spill cost to process. Any block whose heuristic spill cost
      // is larger than this value will not be processed by the optimal scheduler
      // If this field is set to 0, there will be no limit; all blocks will be
      // processed by the optimal scheduler
      int maxSpillCost;
      // An array of possible OptSched heuristic names
      char hurstcNames[HEUR_NAME_CNT][HEUR_NAME_MAX_SIZE];
   		// The algorithm to use for determining the lower bound. Valid values are
      LB_ALG lowerBoundAlgorithm;
      // The heuristic used for the list scheduler.
      SchedPriorities heuristicPriorities;
      // The heuristic used for the enumerator.
      SchedPriorities enumPriorities;
      // Load config files for the OptScheduler and set flags
      void loadOptSchedConfig();
			// Get lower bound algorithm
			LB_ALG parseLowerBoundAlgorithm() const;
      // Get spill cost function
      SPILL_COST_FUNCTION parseSpillCostFunc() const;
      // Return true if the OptScheduler should be enabled for the function this
      // ScheduleDAG was created for
      bool isOptSchedEnabled() const;
      // get latency precision setting
      LATENCY_PRECISION fetchLatencyPrecision() const;
      // Get OptSched heuristic setting
      SchedPriorities parseHeuristic(const std::string &str) const;

    public:
      ScheduleDAGOptSched(llvm::MachineSchedContext* C);
      ~ScheduleDAGOptSched() {}
      // The fallback LLVM scheduler
      void defaultScheduler();
      // Schedule the current region using the OptScheduler
      void schedule() override;
  };

} // namespace opt_sched
#endif
