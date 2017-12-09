#include <iostream>
#include <iomanip>
#include <sstream>
#include "llvm/CodeGen/OptSched/aco.h"
#include "llvm/CodeGen/OptSched/basic/data_dep.h"
#include "llvm/CodeGen/OptSched/basic/ready_list.h"
#include "llvm/CodeGen/OptSched/basic/register.h"
#include "llvm/CodeGen/OptSched/sched_region/sched_region.h"
#include "llvm/CodeGen/OptSched/generic/random.h"

namespace opt_sched {

static void PrintInstruction(SchedInstruction *inst);
void PrintSchedule(InstSchedule *schedule);

double RandDouble(double min, double max) {
  double rand = (double) RandomGen::GetRand32() / UINT32_MAX;
  return (rand * (max - min)) + min;
}

#define INITIAL_VALUE 0.1
#define DECAY_FACTOR 0.5
#define ANTS_PER_ITERATION count_
#define HEURISTIC_IMPORTANCE 2

ACOScheduler::ACOScheduler(DataDepGraph *dataDepGraph, MachineModel *machineModel, InstCount upperBound, SchedPriorities priorities) : ConstrainedScheduler(dataDepGraph, machineModel, upperBound) {
  prirts_ = priorities;
  rdyLst_ = new ReadyList(dataDepGraph_, priorities);
  count_ = dataDepGraph->GetInstCnt();

  int pheremone_size = (count_ + 1) * count_;
  pheremone_ = new pheremone_t[pheremone_size];
  for (int i = 0; i < pheremone_size; i++) {
    // According to Dorigo, a good initial value for the pheremone is slightly
    // more than the average deposited in one iteration. He gives an example
    // for how to compute that for the TSP, I have yet to translate that to
    // this problem.
    pheremone_[i] = INITIAL_VALUE;
  }
}

ACOScheduler::~ACOScheduler() {
  delete rdyLst_;
  delete[] pheremone_;
}

// Pheremone table lookup
// -1 means no instruction, so e.g. pheremone(-1, 10) gives pheremone on path
// from empty schedule to schedule only containing instruction 10
pheremone_t &ACOScheduler::Pheremone(SchedInstruction *from, SchedInstruction *to) {
  assert(to != NULL);
  int fromNum = -1; 
  if (from != NULL)
    fromNum = from->GetNum();
  return Pheremone(fromNum, to->GetNum());
}

pheremone_t &ACOScheduler::Pheremone(InstCount from, InstCount to) {
  int row = 0;
  if (from != -1)
    row = from + 1;
  return pheremone_[(row * count_) + to];
}

double ACOScheduler::Score(SchedInstruction *from, Choice choice) {
  return Pheremone(from, choice.inst) * pow(1.0/(choice.heuristic+1), HEURISTIC_IMPORTANCE);
}

SchedInstruction *ACOScheduler::SelectInstruction(std::vector<Choice> ready, SchedInstruction *lastInst) {
  pheremone_t sum = 0;
  for (auto choice : ready)
    sum += Score(lastInst, choice);
  pheremone_t point = RandDouble(0, sum);
  for (auto choice : ready) {
    point -= Score(lastInst, choice);
    if (point <= 0.00001)
      return choice.inst;
  }
  assert(false); // should not get here
}

void ACOScheduler::UpdatePheremone(InstSchedule *schedule) {
  // I wish InstSchedule allowed you to just iterate over it, but it's got this
  // cycle and slot thing which needs to be accounted for
  InstCount instNum, cycleNum, slotNum;
  instNum = schedule->GetFrstInst(cycleNum, slotNum);

  SchedInstruction *lastInst = NULL;
  while (instNum != INVALID_VALUE) {
    SchedInstruction *inst = dataDepGraph_->GetInstByIndx(instNum);

    pheremone_t &pheremone = Pheremone(lastInst, inst);
    pheremone = pheremone + 1/((double) schedule->GetSpillCost() + 0.1);
    lastInst = inst;

    instNum = schedule->GetNxtInst(cycleNum, slotNum);
  }
  schedule->ResetInstIter();

  // decay pheremone
  for (int i = 0; i < count_; i++) {
    for (int j = 0; j < count_; j++) {
      Pheremone(i, j) *= (1 - DECAY_FACTOR);
    }
  }
  /* PrintPheremone(); */
}

InstSchedule *ACOScheduler::FindOneSchedule() {
  SchedInstruction *lastInst = NULL;
  InstSchedule *schedule = new InstSchedule(machMdl_, dataDepGraph_, true);
  Initialize_();

  while (!IsSchedComplete_()) {
    // convert the ready list from a custom priority queue to a std::vector,
    // much nicer for this particular scheduler
    UpdtRdyLst_(crntCycleNum_, crntSlotNum_);
    std::vector<Choice> ready;
    unsigned long heuristic;
    SchedInstruction *inst = rdyLst_->GetNextPriorityInst(heuristic);
    while (inst != NULL) {
      if (ChkInstLglty_(inst))
        ready.push_back(Choice(inst, heuristic));
      inst = rdyLst_->GetNextPriorityInst(heuristic);
    }
    rdyLst_->ResetIterator();

    // print out the ready list for debugging
    std::stringstream stream;
    stream << "Ready list: ";
    for (auto choice : ready) {
      stream << choice.inst->GetNum() << ", ";
    }
    /* Logger::Info(stream.str().c_str()); */

    inst = NULL;
    if (!ready.empty())
      inst = SelectInstruction(ready, lastInst);
    lastInst = inst;

    // boilerplate, mostly copied from ListScheduler, try not to touch it
    InstCount instNum;
    if (inst == NULL) {
      instNum = SCHD_STALL;
    } else {
      instNum = inst->GetNum();
      SchdulInst_(inst, crntCycleNum_);
      inst->Schedule(crntCycleNum_, crntSlotNum_);
      rgn_->SchdulInst(inst, crntCycleNum_, crntSlotNum_, false);
      DoRsrvSlots_(inst);
      // this is annoying
      SchedInstruction *blah = rdyLst_->GetNextPriorityInst();
      while (blah != NULL && blah != inst) {
        blah = rdyLst_->GetNextPriorityInst();
      }
      if (blah == inst)
        rdyLst_->RemoveNextPriorityInst();
      UpdtSlotAvlblty_(inst);
    }
    /* Logger::Info("Chose instruction %d (for some reason)", instNum); */
    schedule->AppendInst(instNum);
    if (MovToNxtSlot_(inst))
      InitNewCycle_();
    rdyLst_->ResetIterator();
  }
  rgn_->UpdateScheduleCost(schedule);
  return schedule;
}

FUNC_RESULT ACOScheduler::FindSchedule(InstSchedule *schedule_out, SchedRegion *region) {
  // print the thing to be scheduled
  /* std::cerr << "Basic block:" << std::endl; */
  /* for (int i = 0; i < count_; i++) { */
  /*   PrintInstruction(dataDepGraph_->GetInstByIndx(i)); */
  /* } */

  rgn_ = region;
  InstSchedule *bestSchedule = NULL;
  int noChange = 0; // how many iterations with generating the exact same schedule
  for (int it = 0; it < 5 * count_; it++) {
    InstSchedule *iterationBest = NULL;
    for (int i = 0; i < ANTS_PER_ITERATION; i++) {
      InstSchedule *schedule = FindOneSchedule();
      if (iterationBest == NULL || schedule->GetCost() < iterationBest->GetCost()) {
        delete iterationBest;
        iterationBest = schedule;
      } else {
        delete schedule;
      }
    }
    UpdatePheremone(iterationBest);
    PrintSchedule(iterationBest);
    if (bestSchedule && *iterationBest == *bestSchedule) {
      noChange++;
      if (noChange > 10)
        break;
    } else {
      noChange = 0;
    }
    // TODO DRY
    if (bestSchedule == NULL || iterationBest->GetCost() <= bestSchedule->GetCost()) {
      delete bestSchedule;
      bestSchedule = iterationBest;
    } else {
      delete iterationBest;
    }
  }
  /* Logger::Info("best schedule has cost %d\n", bestSchedule->GetCost()); */
  schedule_out->Copy(bestSchedule);
  delete bestSchedule;

  return RES_SUCCESS;
}

// copied from Enumerator
inline void ACOScheduler::UpdtRdyLst_(InstCount cycleNum, int slotNum) {
  InstCount prevCycleNum = cycleNum - 1;
  LinkedList<SchedInstruction> *lst1 = NULL;
  LinkedList<SchedInstruction> *lst2 = frstRdyLstPerCycle_[cycleNum];

  if (slotNum == 0 && prevCycleNum >= 0) {
    // If at the begining of a new cycle other than the very first cycle, then
    // we also have to include the instructions that might have become ready in
    // the previous cycle due to a zero latency of the instruction scheduled in
    // the very last slot of that cycle [GOS 9.8.02].
    lst1 = frstRdyLstPerCycle_[prevCycleNum];

    if (lst1 != NULL) {
      rdyLst_->AddList(lst1);
      lst1->Reset();
      CleanupCycle_(prevCycleNum);
    }
  }

  if (lst2 != NULL) {
    rdyLst_->AddList(lst2);
    lst2->Reset();
  }
}

void ACOScheduler::PrintPheremone() {
  for (int i = 0; i < count_; i++) {
    for (int j = 0; j < count_; j++) {
      std::cerr << std::fixed << std::setprecision(2) << Pheremone(i,j) << " ";
    }
    std::cerr << std::endl;
  }
  std::cerr << std::endl;
}

static void PrintInstruction(SchedInstruction *inst) {
  std::cerr << std::setw(2) << inst->GetNum() << " ";
  std::cerr << std::setw(20) << std::left << inst->GetOpCode();

  std::cerr << " defs ";
  Register **defs;
  uint16_t defsCount = inst->GetDefs(defs);
  for (uint16_t i = 0; i < defsCount; i++) {
    std::cerr << defs[i]->GetNum() << defs[i]->GetType();
    if (i != defsCount - 1)
      std::cerr << ", ";
  }

  std::cerr << " uses ";
  Register **uses;
  uint16_t usesCount = inst->GetUses(uses);
  for (uint16_t i = 0; i < usesCount; i++) {
    std::cerr << uses[i]->GetNum() << uses[i]->GetType();
    if (i != usesCount - 1)
      std::cerr << ", ";
  }
  std::cerr << std::endl;
}

void PrintSchedule(InstSchedule *schedule) {
  std::cerr << schedule->GetSpillCost() << ": ";
  InstCount instNum, cycleNum, slotNum;
  instNum = schedule->GetFrstInst(cycleNum, slotNum);
  while (instNum != INVALID_VALUE) {
    std::cerr << instNum << " ";
    instNum = schedule->GetNxtInst(cycleNum, slotNum);
  }
  std::cerr << std::endl;
  schedule->ResetInstIter();
}

}
