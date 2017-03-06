/*******************************************************************************
Description:  A wrapper that convert an LLVM target to an OptSched MachineModel.
Author:       Max Shawabkeh
Created:      Mar. 2011
Last Update:  Mar. 2017
*******************************************************************************/

#include "llvm/CodeGen/OptSched/OptSchedMachineWrapper.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/OptSched/generic/logger.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "optsched"

namespace opt_sched {

using namespace llvm;

LLVMMachineModel::LLVMMachineModel(llvm::MachineSchedContext* context, const string configFile) : MachineModel(configFile) {
  const TargetMachine& target = context->PassConfig->template getTM<TargetMachine>();

  mdlName_ = target.getTarget().getName();

  Logger::Info("Machine model: %s", mdlName_.c_str());

 // Clear The registerTypes list to read registers limits from the LLVM machine model
  registerTypes_.clear();

  // TODO(max99x): Improve register pressure limit estimates.
  const MCRegisterInfo* regInfo = target.getMCRegisterInfo();
  for (MCRegisterInfo::regclass_iterator cls = regInfo->regclass_begin();
       cls != regInfo->regclass_end();
       cls++) {
    RegTypeInfo regType;
    regType.name = regInfo->getRegClassName(cls);
    regType.count = cls->getNumRegs();
    // HACK: Special case for the x86 flags register.
    if (mdlName_.find("x86") == 0 && (regType.name == "CCR" || regType.name == "FPCCR")) {
      regType.count = 1;
    }
    if (regType.count > 0) {
      // Only count types with non-zero limits.
      regClassToType_[cls] = registerTypes_.size();
      regTypeToClass_[registerTypes_.size()] = cls;
      registerTypes_.push_back(regType);
      Logger::Info("Reg Type %s has a limit of %d",regType.name.c_str(), regType.count);
    }
  }

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
  #ifdef IS_DEBUG_MACHINE_MODEL
        Logger::Info("######################## THE MACHINE MODEL #######################");
        Logger::Info("Issue Rate: %d. Issue Slot Count: %d", issueRate_, issueSlotCnt_);
        Logger::Info("Issue Types Count: %d", issueTypes_.size());
        for(int x = 0; x < issueTypes_.size(); x++)
                Logger::Info("Type %s has %d slots", issueTypes_[x].name.c_str(), issueTypes_[x].slotsCount);
        Logger::Info("Instructions Type Count: %d", instTypes_.size());
        for(int y = 0; y < instTypes_.size(); y++)
                Logger::Info("Instruction %s is of issue type %s and has a latency of %d", 
                                                instTypes_[y].name.c_str(), issueTypes_[instTypes_[y].issuType].name.c_str(),
                                                instTypes_[y].ltncy);
  #endif
}

int LLVMMachineModel::GetRegType(const llvm::MCRegisterClass* cls,
                                 const llvm::MCRegisterInfo* regInfo) const {
  // HACK: Map x86 virtual RFP registers to VR128.
  if (mdlName_.find("x86") == 0 && std::string(regInfo->getRegClassName(cls), 3) == "RFP") {
    Logger::Info("Mapping RFP into VR128");
    return GetRegTypeByName("VR128");
  }
  assert(regClassToType_.find(cls) != regClassToType_.end());
  return regClassToType_.find(cls)->second;
}

const llvm::MCRegisterClass* LLVMMachineModel::GetRegClass(int type) const {
  assert(regTypeToClass_.find(type) != regTypeToClass_.end());
  return regTypeToClass_.find(type)->second;
}

} // end namespace opt_sched
