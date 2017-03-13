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

    int GetRegType(const llvm::MCRegisterClass* cls, 
                   const llvm::MCRegisterInfo* regInfo) const;
    const llvm::MCRegisterClass* GetRegClass(int type) const;

  protected:
    std::map<const llvm::MCRegisterClass*, int> regClassToType_;
    std::map<int, const llvm::MCRegisterClass*> regTypeToClass_;

};

} // end namespace opt_sched

#endif
