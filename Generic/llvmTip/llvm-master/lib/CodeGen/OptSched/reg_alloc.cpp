#include "llvm/CodeGen/OptSched/basic/data_dep.h"
#include "llvm/CodeGen/OptSched/basic/reg_alloc.h"
#include "llvm/CodeGen/OptSched/basic/register.h"
#include "llvm/CodeGen/OptSched/basic/sched_basic_data.h"
#include "llvm/CodeGen/OptSched/generic/logger.h"
#include <climits>
#include <utility>

namespace opt_sched {

LocalRegAlloc::LocalRegAlloc(InstSchedule *instSchedule,
                             DataDepGraph *dataDepGraph) {
  instSchedule_ = instSchedule;
  dataDepGraph_ = dataDepGraph;
}

LocalRegAlloc::~LocalRegAlloc() {}

void LocalRegAlloc::AllocRegs() {
#ifdef IS_DEBUG
  Logger::Info("REG_ALLOC: Starting LocalRegAlloc.");
#endif

  InstCount cycle, slot;
  for (InstCount i = instSchedule_->GetFrstInst(cycle, slot);
       i != INVALID_VALUE; i = instSchedule_->GetNxtInst(cycle, slot)) {
    int instNum = i;
#ifdef IS_DEBUG_REG_ALLOC
    Logger::Info("REG_ALLOC: Processing instruction %d.", instNum);
#endif
    SchedInstruction *inst = dataDepGraph_->GetInstByIndx(instNum);
    Register **uses;
    Register **defs;
    int useCnt = inst->GetUses(uses);
    int defCnt = inst->GetDefs(defs);

    // Find registers for this instruction's VReg uses.
    for (int u = 0; u < useCnt; u++) {
      Register *use = uses[u];
      int16_t regType = use->GetType();
      int virtRegNum = use->GetNum();
      RegMap &map = regMaps_[regType][virtRegNum];
#ifdef IS_DEBUG_REG_ALLOC
      Logger::Info("REG_ALLOC: Processing use for register %d:%d.", regType,
                   virtRegNum);
#endif

      if (map.assignedReg == -1) {
#ifdef IS_DEBUG_REG_ALLOC
        Logger::Info("REG_ALLOC: Adding load for register %d:%d.", regType,
                     virtRegNum);
#endif
        numLoads_++;
        AllocateReg_(regType, virtRegNum);
      }
    }

    // Free physical registers if this is the last use for them.
    for (int u = 0; u < useCnt; u++) {
      Register *use = uses[u];
      int16_t regType = use->GetType();
      int virtRegNum = use->GetNum();
      RegMap &map = regMaps_[regType][virtRegNum];
      std::vector<int> &physRegs = physRegs_[regType];

      assert(map.nextUses.front() == instNum);
      map.nextUses.pop();

      if (map.nextUses.empty() && map.assignedReg != -1) {
        int physRegNum = map.assignedReg;
        assert(physRegs[physRegNum] == virtRegNum);
        map.assignedReg = -1;
        physRegs[physRegNum] = -1;
        freeRegs_[regType].push(physRegNum);
      }
    }

    // Process definitions.
    for (int d = 0; d < defCnt; d++) {
      Register *def = defs[d];
      int16_t regType = def->GetType();
      int virtRegNum = def->GetNum();
#ifdef IS_DEBUG_REG_ALLOC
      Logger::Info("REG_ALLOC: Processing def for register %d:%d.", regType,
                   virtRegNum);
#endif
      AllocateReg_(regType, virtRegNum);
    }
  }
}

void LocalRegAlloc::AllocateReg_(int16_t regType, int virtRegNum) {
  std::map<int, RegMap> &regMaps = regMaps_[regType];
  std::stack<int> &free = freeRegs_[regType];
  std::vector<int> &physRegs = physRegs_[regType];
  int physRegNum = -1;

  if (!free.empty()) {
    physRegNum = free.top();
    regMaps[virtRegNum].assignedReg = free.top();
    physRegs[physRegNum] = virtRegNum;
    free.pop();
  } else {
    int virtRegWithMaxUse = FindMaxNextUse_(regMaps, physRegs);
#ifdef IS_DEBUG_REG_ALLOC
    Logger::Info("REG_ALLOC: Adding store for register %d:%d.", regType,
                 virtRegWithMaxUse);
#endif
    numStores_++;
    physRegNum = regMaps[virtRegWithMaxUse].assignedReg;
    assert(physRegNum != -1);
    regMaps[virtRegWithMaxUse].assignedReg = -1;
    regMaps[virtRegNum].assignedReg = physRegNum;
    physRegs[physRegNum] = virtRegNum;
  }
#ifdef IS_DEBUG_REG_ALLOC
  Logger::Info("REG_ALLOC: Mapping virtual register %d:%d to %d:%d", regType,
               virtRegNum, regType, physRegNum);
#endif
}

int LocalRegAlloc::FindMaxNextUse_(std::map<int, RegMap> &regMaps,
                                   std::vector<int> &physRegs) {
  int max = INT_MIN;
  int virtRegWithMaxUse = -1;
  for (int i = 0; i < physRegs.size(); i++) {
    int virtReg = physRegs[i];
    int next = instSchedule_->GetSchedCycle(regMaps[virtReg].nextUses.front());
    if (next > max) {
      max = next;
      virtRegWithMaxUse = virtReg;
    }
  }
  assert(virtRegWithMaxUse != -1);
#ifdef IS_DEBUG_REG_ALLOC
  Logger::Info("REG_ALLOC: Register with the latest use %d.",
               virtRegWithMaxUse);
#endif
  return virtRegWithMaxUse;
}

void LocalRegAlloc::SetupForRegAlloc() {
  numLoads_ = 0;
  numStores_ = 0;
  numRegTypes_ = dataDepGraph_->GetRegTypeCnt();
#ifdef IS_DEBUG_REG_ALLOC
  Logger::Info("REG_ALLOC: Found %d register types.", numRegTypes_);
#endif

  // Initialize a free register stack for each register type
  freeRegs_.resize(numRegTypes_);
  physRegs_.resize(numRegTypes_);
  for (int i = 0; i < numRegTypes_; i++)
    for (int j = 0; j < dataDepGraph_->GetPhysRegCnt(i); j++) {
      freeRegs_[i].push(j);
      physRegs_[i].push_back(-1);
    }

  // Initialize list of register's next uses.
  vector<vector<stack<int>>> nextUse_;
  regMaps_.resize(numRegTypes_);
  ScanUses_();
}

void LocalRegAlloc::ScanUses_() {
  InstCount cycle, slot;
  for (InstCount i = instSchedule_->GetFrstInst(cycle, slot);
       i != INVALID_VALUE; i = instSchedule_->GetNxtInst(cycle, slot)) {
    SchedInstruction *inst = dataDepGraph_->GetInstByIndx(i);
    int instNum = i;
    Register **uses;
    int useCnt = inst->GetUses(uses);
#ifdef IS_DEBUG_REG_ALLOC
    Logger::Info("REG_ALLOC: Scanning for uses for instruction %d.", instNum);
#endif

    for (int j = 0; j < useCnt; j++) {
      Register *use = uses[j];
      int virtRegNum = use->GetNum();
      int16_t regType = use->GetType();
      if (regMaps_[regType].find(virtRegNum) == regMaps_[regType].end()) {
        RegMap m;
        m.nextUses.push(instNum);
        m.assignedReg = -1;
        regMaps_[regType][virtRegNum] = m;
      } else {
        regMaps_[regType][virtRegNum].nextUses.push(instNum);
      }
#ifdef IS_DEBUG_REG_ALLOC
      Logger::Info("REG_ALLOC: Adding use for instruction %d.", instNum);
      Logger::Info("REG_ALLOC: Use list for register %d:%d is now:", regType,
                   virtRegNum);
      std::queue<int> next = regMaps_[regType][virtRegNum].nextUses;
      while (!next.empty()) {
        Logger::Info("%d", next.front());
        next.pop();
      }
#endif
    }
  }
}

void LocalRegAlloc::PrintSpillInfo(const char *dagName) {
  Logger::Info("OPT_SCHED LOCAL RA: DAG Name: %s Number of spills: %d", dagName,
               numLoads_ + numStores_);
  Logger::Info("Number of stores %d", numStores_);
  Logger::Info("Number of loads %d", numLoads_);
}

int LocalRegAlloc::GetCost() { return numLoads_ + numStores_; }

} // end namespace opt_sched
