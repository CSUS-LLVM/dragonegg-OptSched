// Ant colony optimizing scheduler

#ifndef OPTSCHED_ACO_H
#define OPTSCHED_ACO_H

#include "llvm/CodeGen/OptSched/basic/gen_sched.h"

namespace opt_sched {

typedef double pheremone_t;

struct Choice {
  SchedInstruction *inst;
  unsigned long heuristic;
  Choice(SchedInstruction *_inst, unsigned long _heuristic) 
    : inst(_inst), heuristic(_heuristic) {}
};

class ACOScheduler : public ConstrainedScheduler {
public:
  ACOScheduler(DataDepGraph *dataDepGraph, MachineModel *machineModel, InstCount upperBound, SchedPriorities priorities);
  virtual ~ACOScheduler();
  FUNC_RESULT FindSchedule(InstSchedule *schedule, SchedRegion *region);
  inline void UpdtRdyLst_(InstCount cycleNum, int slotNum);
private:
  pheremone_t &Pheremone(SchedInstruction *from, SchedInstruction *to);
  pheremone_t &Pheremone(InstCount from, InstCount to);
  double Score(SchedInstruction *from, Choice choice);

  void PrintPheremone();

  SchedInstruction *SelectInstruction(std::vector<Choice> ready, SchedInstruction *lastInst);
  void UpdatePheremone(InstSchedule *schedule);
  InstSchedule *FindOneSchedule();
  pheremone_t *pheremone_;
  int count_;
};

}

#endif
