/*******************************************************************************
Description:  A wrapper that convert an LLVM target to an OptSched MachineModel.
Author:       Max Shawabkeh
Created:      Mar. 2011
Last Update:  Mar. 2017
*******************************************************************************/

#ifndef OPTSCHED_MACHINE_MODEL_WRAPPER_H
#define OPTSCHED_MACHINE_MODEL_WRAPPER_H

#include "llvm/CodeGen/OptSched/basic/machine_model.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include <map>

namespace opt_sched {

class LLVMMachineModel : public MachineModel {
  public:
    LLVMMachineModel(llvm::MachineSchedContext* context, const string configFile);
    ~LLVMMachineModel() {}

    int GetRegType(const llvm::TargetRegisterClass* cls) const;
    const llvm::TargetRegisterClass* GetRegClass(int type) const;

  protected:
    std::map<const llvm::TargetRegisterClass*, int> regClassToType_;
    std::map<int, const llvm::TargetRegisterClass*> regTypeToClass_;

};

} // end namespace opt_sched

#endif
