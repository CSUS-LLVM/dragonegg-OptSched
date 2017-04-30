/*******************************************************************************
Description:  A wrapper that convert an LLVM target to an OptSched MachineModel.
Author:       Max Shawabkeh
Created:      Mar. 2011
Last Update:  Mar. 2017
*******************************************************************************/

#include "llvm/CodeGen/OptSched/OptSchedMachineWrapper.h"
#include "llvm/CodeGen/OptSched/generic/logger.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetRegisterInfo.h"

#define DEBUG_TYPE "optsched"

namespace opt_sched {

using namespace llvm;

LLVMMachineModel::LLVMMachineModel(const string configFile)
    : MachineModel(configFile) {}

void LLVMMachineModel::convertMachineModel(
    const ScheduleDAGMILive *scheduleDag,
    const RegisterClassInfo *regClassInfo) {
  const TargetMachine &target = scheduleDag->TM;

  mdlName_ = target.getTarget().getName();

  Logger::Info("Machine model: %s", mdlName_.c_str());

  // Clear The registerTypes list to read registers limits from the LLVM machine
  // model
  registerTypes_.clear();

  registerInfo = scheduleDag->TRI;
  for (int pSet = 0; pSet < registerInfo->getNumRegPressureSets(); ++pSet) {
    RegTypeInfo regType;
    regType.name = registerInfo->getRegPressureSetName(pSet);
    int pressureLimit = regClassInfo->getRegPressureSetLimit(pSet);
    // set registers with 0 limit to 1 to support flags and special cases
    if (pressureLimit > 0)
      regType.count = pressureLimit;
    else
      regType.count = 1;
    registerTypes_.push_back(regType);
#ifdef IS_DEBUG_MM
    Logger::Info("Pressure set %s has a limit of %d", regType.name.c_str(),
                 regType.count);
#endif
  }

#ifdef IS_DEBUG_REG_CLASS
  Logger::Info("LLVM register class info");

  for (TargetRegisterClass::sc_iterator cls = registerInfo->regclass_begin();
       cls != registerInfo->regclass_end(); cls++) {
    RegTypeInfo regType;
    const char *clsName = registerInfo->getRegClassName(*cls);
    unsigned weight = registerInfo->getRegClassWeight(*cls).RegWeight;
    Logger::Info("For the register class %s getNumRegs is %d", clsName,
                 (*cls)->getNumRegs());
    Logger::Info("For the register class %s getRegPressureLimit is %d", clsName,
                 registerInfo->getRegPressureLimit((*cls), scheduleDag->MF));
    Logger::Info("This register has a weight of %lu", weight);
    Logger::Info("Pressure Sets for this register class");
    for (const int *pSet = registerInfo->getRegClassPressureSets(*cls);
         *pSet != -1; ++pSet) {
      Logger::Info(
          "Pressure set %s has member %s and a pressure set limit of %d",
          registerInfo->getRegPressureSetName(*pSet), clsName,
          registerInfo->getRegPressureSetLimit(scheduleDag->MF, *pSet));
    }
  }
#endif

  // TODO(max99x): Get real instruction types.
  InstTypeInfo instType;

  instType.name = "Default";
  instType.isCntxtDep = false;
  instType.issuType = 0;
  instType.ltncy = 1;
  instType.pipelined = true;
  instType.sprtd = true;
  instType.blksCycle = false;
  instTypes_.push_back(instType);

  instType.name = "artificial";
  instType.isCntxtDep = false;
  instType.issuType = 0;
  instType.ltncy = 1;
  instType.pipelined = true;
  instType.sprtd = true;
  instType.blksCycle = false;
  instTypes_.push_back(instType);

// Print the machine model parameters.
#ifdef IS_DEBUG_MM
  Logger::Info(
      "######################## THE MACHINE MODEL #######################");
  Logger::Info("Issue Rate: %d. Issue Slot Count: %d", issueRate_,
               issueSlotCnt_);
  Logger::Info("Issue Types Count: %d", issueTypes_.size());
  for (int x = 0; x < issueTypes_.size(); x++)
    Logger::Info("Type %s has %d slots", issueTypes_[x].name.c_str(),
                 issueTypes_[x].slotsCount);

  Logger::Info("Instructions Type Count: %d", instTypes_.size());
  for (int y = 0; y < instTypes_.size(); y++)
    Logger::Info("Instruction %s is of issue type %s and has a latency of %d",
                 instTypes_[y].name.c_str(),
                 issueTypes_[instTypes_[y].issuType].name.c_str(),
                 instTypes_[y].ltncy);
#endif
}

} // end namespace opt_sched
