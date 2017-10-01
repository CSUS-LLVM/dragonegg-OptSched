/*******************************************************************************
Description:  A wrapper that convert an LLVM ScheduleDAG to an OptSched
              DataDepGraph.
*******************************************************************************/

#include "llvm/CodeGen/OptSched/OptSchedDagWrapper.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/OptSched/basic/register.h"
#include "llvm/CodeGen/OptSched/basic/sched_basic_data.h"
#include "llvm/CodeGen/OptSched/generic/config.h"
#include "llvm/CodeGen/OptSched/generic/logger.h"
#include "llvm/CodeGen/RegisterPressure.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Function.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetMachine.h"
#include <cstdio>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <vector>

#define DEBUG_TYPE "optsched"

namespace opt_sched {

using namespace llvm;

LLVMDataDepGraph::LLVMDataDepGraph(
    MachineSchedContext *context, ScheduleDAGMILive *llvmDag,
    LLVMMachineModel *machMdl, LATENCY_PRECISION ltncyPrcsn,
    MachineBasicBlock *BB, GraphTransTypes graphTransTypes,
    ScheduleDAGTopologicalSort &Topo, bool treatOrderDepsAsDataDeps,
    int maxDagSizeForPrcisLtncy, int regionNum)
    : DataDepGraph(machMdl, ltncyPrcsn, graphTransTypes),
      llvmNodes_(llvmDag->SUnits), context_(context), schedDag_(llvmDag),
      topo_(Topo), target_(llvmDag->TM) {
  llvmMachMdl_ = static_cast<LLVMMachineModel *>(machMdl_);
  dagFileFormat_ = DFF_BB;
  isTraceFormat_ = false;
  ltncyPrcsn_ = ltncyPrcsn;
  treatOrderDepsAsDataDeps_ = treatOrderDepsAsDataDeps;
  maxDagSizeForPrcisLtncy_ = maxDagSizeForPrcisLtncy;

  // The extra 2 are for the artifical root and leaf nodes.
  instCnt_ = nodeCnt_ = llvmNodes_.size() + 2;

  // TODO(max99x): Find real weight.
  weight_ = 1.0f;

  std::snprintf(dagID_, MAX_NAMESIZE, "%s:%d",
                context_->MF->getFunction()->getName().data(), regionNum);

  std::snprintf(compiler_, MAX_NAMESIZE, "LLVM");

  AllocArrays_(instCnt_);

  includesNonStandardBlock_ = false;
  includesUnsupported_ = false;

  // TODO(max99x)/(austin): Find real value for this.
  includesUnpipelined_ = true;

  ConvertLLVMNodes_();

  if (Finish_() == RES_ERROR)
    Logger::Fatal("DAG Finish_() failed.");
}

void LLVMDataDepGraph::ConvertLLVMNodes_() {
  includesCall_ = false;

  std::vector<int> roots;
  std::vector<int> leaves;

  InstType instType;
  std::string instName;
  std::string opCode;
  int ltncy;

#ifdef IS_DEBUG
  Logger::Info("Building opt_sched DAG out of llvm DAG");
#endif

  // Create nodes.
  for (size_t i = 0; i < llvmNodes_.size(); i++) {

    const SUnit &unit = llvmNodes_[i];
#ifdef IS_DEBUG_DAG
    unit.dumpAll(schedDag_);
#endif
    // Make sure this is a real node
    if (unit.isBoundaryNode() || !unit.isInstr())
      continue;

    const MachineInstr *instr = unit.getInstr();

    // Make sure nodes are in numbered order.
    assert(unit.NodeNum == i);

    instName = opCode = schedDag_->TII->getName(instr->getOpcode());

    // Search in the machine model for an instType with this OpCode name
    instType = machMdl_->GetInstTypeByName(instName.c_str());

    // If the machine model does not have instType with this OpCode name, use
    // the default instType
    if (instType == INVALID_INST_TYPE) {
#ifdef IS_DEBUG_DAG
      Logger::Info(
          "Instruction %s was not found in machine model. Using the default",
          instName.c_str());
#endif
      instName = "Default";
      instType = machMdl_->GetInstTypeByName("Default");
    }

    CreateNode_(unit.NodeNum, instName.c_str(), instType, opCode.c_str(),
                unit.NodeNum, // nodeID
                0, 0,
                0,  // fileInstLwrBound
                0,  // fileInstUprBound
                0); // blkNum
    if (unit.isCall)
      includesCall_ = true;
    if (isRootNode(unit)) {
      roots.push_back(unit.NodeNum);
#ifdef IS_DEBUG_BUILD_DAG
      Logger::Info("Pushing root node: %d", unit.NodeNum);
#endif
    }
    if (isLeafNode(unit)) {
      leaves.push_back(unit.NodeNum);
#ifdef IS_DEBUG_BUILD_DAG
      Logger::Info("Pushing leaf node: %d", unit.NodeNum);
#endif
    }
  }

  // Create edges.
  for (size_t i = 0; i < llvmNodes_.size(); i++) {
    const SUnit &unit = llvmNodes_[i];
    const MachineInstr *instr = unit.getInstr();
    for (SUnit::const_succ_iterator it = unit.Succs.begin();
         it != unit.Succs.end(); it++) {
      // check if the successor is a boundary node
      if (it->getSUnit()->isBoundaryNode())
        continue;

      DependenceType depType;
      switch (it->getKind()) {
      case SDep::Data:
        depType = DEP_DATA;
        break;
      case SDep::Anti:
        depType = DEP_ANTI;
        break;
      case SDep::Output:
        depType = DEP_OUTPUT;
        break;
      case SDep::Order:
        depType = treatOrderDepsAsDataDeps_ ? DEP_DATA : DEP_OTHER;
        break;
      }

      LATENCY_PRECISION prcsn = ltncyPrcsn_;
      if (prcsn == LTP_PRECISE && maxDagSizeForPrcisLtncy_ > 0 &&
          llvmNodes_.size() > maxDagSizeForPrcisLtncy_)
        prcsn = LTP_ROUGH; // use rough latencies if DAG is too large

      if (prcsn == LTP_PRECISE) { // if precise latency, get the precise latency
                                  // from the machine model
        instName = schedDag_->TII->getName(instr->getOpcode());
        instType = machMdl_->GetInstTypeByName(instName);
        ltncy = machMdl_->GetLatency(instType, depType);

#ifdef IS_DEBUG_BUILD_DAG
        Logger::Info("Dep type %d with latency %d from Instruction %s", depType,
                     ltncy, instName.c_str());
#endif
      } else if (prcsn == LTP_ROUGH) { // use the compiler's rough latency
        ltncy = it->getLatency();
      } else
        ltncy = 1;

      CreateEdge_(unit.NodeNum, it->getSUnit()->NodeNum, ltncy, depType);

#ifdef IS_DEBUG_BUILD_DAG
      Logger::Info("Creating an edge from %d to %d. Type is %d, latency = %d",
                   unit.NodeNum, it->getSUnit()->NodeNum, depType, ltncy);
#endif
    }
  }

  // add edges between equivalent instructions

  size_t maxNodeNum = llvmNodes_.size() - 1;

  // Create artificial root.
  assert(roots.size() > 0 && leaves.size() > 0);
  int rootNum = ++maxNodeNum;
  root_ =
      CreateNode_(rootNum, "artificial",
                  machMdl_->GetInstTypeByName("artificial"), "__optsched_entry",
                  rootNum, // nodeID
                  0,       // fileSchedOrder
                  0,       // fileSchedCycle
                  0,       // fileInstLwrBound
                  0,       // fileInstUprBound
                  0);      // blkNum
  for (size_t i = 0; i < llvmNodes_.size(); i++) {
    if (insts_[i]->GetPrdcsrCnt() == 0)
      CreateEdge_(rootNum, i, 0, DEP_OTHER);
  }

  // Create artificial leaf.
  int leafNum = ++maxNodeNum;
  CreateNode_(leafNum, "artificial", machMdl_->GetInstTypeByName("artificial"),
              "__optsched_exit",
              leafNum, // nodeID
              0, 0,
              0,  // fileInstLwrBound
              0,  // fileInstUprBound
              0); // blkNum
  for (size_t i = 0; i < llvmNodes_.size(); i++) {
    if (insts_[i]->GetScsrCnt() == 0)
      CreateEdge_(i, leafNum, 0, DEP_OTHER);
  }
  AdjstFileSchedCycles_();
  PrintEdgeCntPerLtncyInfo();
}

void LLVMDataDepGraph::CountDefs(RegisterFile regFiles[]) {
  std::vector<int> regDefCounts(machMdl_->GetRegTypeCnt());

  // count live-in as defs in root node
  for (const RegisterMaskPair &L : schedDag_->getRegPressure().LiveInRegs) {
    unsigned resNo = L.RegUnit;

    std::vector<int> regTypes = GetRegisterType_(resNo);
    for (int regType : regTypes)
      regDefCounts[regType]++;
  }

  for (std::vector<SUnit>::iterator it = llvmNodes_.begin();
       it != llvmNodes_.end(); it++) {
    MachineInstr *MI = it->getInstr();
    // Get all defs for this instruction
    RegisterOperands RegOpers;
    RegOpers.collect(*MI, *schedDag_->TRI, schedDag_->MRI, false, true);
    for (const RegisterMaskPair &D : RegOpers.Defs) {
      unsigned resNo = D.RegUnit;

      std::vector<int> regTypes = GetRegisterType_(resNo);
      for (int regType : regTypes)
        regDefCounts[regType]++;
    }
  }

  for (int i = 0; i < machMdl_->GetRegTypeCnt(); i++) {
#ifdef IS_DEBUG_COUNT_DEFS
    if (regDefCounts[i]) {
      Logger::Info("Reg Type %s -> %d registers",
                   llvmMachMdl_->GetRegTypeName(i).c_str(), regDefCounts[i]);
    }
#endif
    regFiles[i].SetRegCnt(regDefCounts[i]);
  }
}

void LLVMDataDepGraph::AddDefsAndUses(RegisterFile regFiles[]) {
  // The index of the last "assigned" register for each register type.
  std::vector<int> regIndices(machMdl_->GetRegTypeCnt());
  // Count each definition of a virtual register with the same resNo
  // as a seperate register in our model. Each resNo is also associated
  // with multiple pressure sets which are treated as seperate registers
  std::map<unsigned, std::vector<Register *>> lastDef;

  // index of root node in insts_
  int rootIndex = llvmNodes_.size();

  // Add live in regs as defs for artificial root
  for (const RegisterMaskPair &I : schedDag_->getRegPressure().LiveInRegs) {
    unsigned resNo = I.RegUnit;
    int weight = GetRegisterWeight_(resNo);
    std::vector<int> regTypes = GetRegisterType_(resNo);

    std::vector<Register *> regs;
    for (int regType : regTypes) {
      Register *reg = regFiles[regType].GetReg(regIndices[regType]++);
      insts_[rootIndex]->AddDef(reg);
      reg->SetWght(weight);
      reg->AddDef(insts_[rootIndex]);
      reg->SetIsLiveIn(true);
#ifdef IS_DEBUG_DEFS_AND_USES
      Logger::Info("Adding live-in def for OptSched register: type: %lu "
                   "number: %lu NodeNum: %lu",
                   reg->GetType(), reg->GetNum(), rootIndex);
#endif
      regs.push_back(reg);
    }
    lastDef[resNo] = regs;

#ifdef IS_DEBUG_DEFS_AND_USES
    Logger::Info("Adding live-in def for LLVM register: %lu NodeNum: %lu",
                 resNo, rootIndex);
#endif
  }

  std::vector<SUnit>::iterator startNode;
  for (startNode = llvmNodes_.begin(); startNode != llvmNodes_.end();
       ++startNode) {
    // The machine instruction we are processing
    MachineInstr *MI = startNode->getInstr();

    // Collect def/use information for this machine instruction
    RegisterOperands RegOpers;
    RegOpers.collect(*MI, *schedDag_->TRI, schedDag_->MRI, false, true);

    // add uses
    for (const RegisterMaskPair &U : RegOpers.Uses) {
      unsigned resNo = U.RegUnit;
      std::vector<int> resTypes = GetRegisterType_(resNo);

      std::vector<Register *> regs = lastDef[resNo];
      for (Register *reg : regs) {
        if (!insts_[rootIndex]->FindUse(reg)) {
          insts_[startNode->NodeNum]->AddUse(reg);
          reg->AddUse(insts_[startNode->NodeNum]);
#ifdef IS_DEBUG_DEFS_AND_USES
          Logger::Info("Adding use for OptSched register: type: %lu number: "
                       "%lu  NodeNum: %lu",
                       reg->GetType(), reg->GetNum(), startNode->NodeNum);
#endif
        }
      }
#ifdef IS_DEBUG_DEFS_AND_USES
      Logger::Info("Adding use for LLVM register: %lu NodeNum: %lu", resNo,
                   startNode->NodeNum);
#endif
    }

    // add defs
    for (const RegisterMaskPair &D : RegOpers.Defs) {
      unsigned resNo = D.RegUnit;
      int weight = GetRegisterWeight_(resNo);
      std::vector<int> regTypes = GetRegisterType_(resNo);

      std::vector<Register *> regs;
      for (int regType : regTypes) {
        Register *reg = regFiles[regType].GetReg(regIndices[regType]++);
        insts_[startNode->NodeNum]->AddDef(reg);
        reg->SetWght(weight);
        reg->AddDef(insts_[startNode->NodeNum]);
#ifdef IS_DEBUG_DEFS_AND_USES
        Logger::Info("Adding def for OptSched register: type: %lu number: %lu "
                     "NodeNum: %lu",
                     reg->GetType(), reg->GetNum(), startNode->NodeNum);
#endif
        regs.push_back(reg);
      }
      lastDef[resNo] = regs;

#ifdef IS_DEBUG_DEFS_AND_USES
      Logger::Info("Adding def for LLVM register: %lu NodeNum: %lu", resNo,
                   startNode->NodeNum);
#endif
    }
  }

  // index of leaf node in insts_
  int leafIndex = llvmNodes_.size() + 1;

  // add live-out registers as uses in artificial leaf instruction
  for (const RegisterMaskPair &O : schedDag_->getRegPressure().LiveOutRegs) {
    unsigned resNo = O.RegUnit;
    std::vector<int> regTypes = GetRegisterType_(resNo);

    std::vector<Register *> regs = lastDef[resNo];
    for (Register *reg : regs) {
      if (!insts_[leafIndex]->FindUse(reg)) {
        insts_[leafIndex]->AddUse(reg);
        reg->AddUse(insts_[leafIndex]);
        reg->SetIsLiveOut(true);
#ifdef IS_DEBUG_DEFS_AND_USES
        Logger::Info("Adding live-out use for OptSched register: type: %lu "
                     "number: %lu NodeNum: %lu",
                     reg->GetType(), reg->GetNum(), leafIndex);
#endif
      }
#ifdef IS_DEBUG_DEFS_AND_USES
      Logger::Info("Adding live-out use for register: %lu NodeNum: %lu", resNo,
                   leafIndex);
#endif
    }
  }

  // Check for any registers that are not used but are also not in LLVM's
  // live-out set.
  // Optionally, add these registers as uses in the aritificial leaf node.
  if (SchedulerOptions::getInstance().GetBool("ADD_DEFINED_AND_NOT_USED_REGS")) {
    for (int i = 0; i < machMdl_->GetRegTypeCnt(); i++) {
      for (int j = 0; j < regFiles[i].GetRegCnt(); j++) {
        Register *reg = regFiles[i].GetReg(j);
        if (reg->GetUseCnt() == 0) {
          if (!insts_[leafIndex]->FindUse(reg)) {
            insts_[leafIndex]->AddUse(reg);
            reg->AddUse(insts_[leafIndex]);
            reg->SetIsLiveOut(true);
#ifdef IS_DEBUG_DEFS_AND_USES
            Logger::Info("Adding live-out use for OptSched register: type: %lu "
                         "This register is not in the live-out set from LLVM"
                         "number: %lu NodeNum: %lu",
                         reg->GetType(), reg->GetNum(), leafIndex);
#endif
          }
#ifdef IS_DEBUG_DEFS_AND_USES
          Logger::Info("Adding live-out use for register: %lu NodeNum: %lu",
                       resNo, leafIndex);
#endif
        }
      }
    }
  }

// (Chris) Debug: Count the number of defs and uses for each register.
// Ensure that any changes to how opt_sched::Register tracks defs and uses
// doesn't change these values.
//
// Also, make sure that iterating through all the registers gives the same use
// and def count as iterating through all the instructions.
#if defined(IS_DEBUG_DEF_USE_COUNT)
  auto regTypeCount = machMdl_->GetRegTypeCnt();
  uint64_t defsFromRegs = 0;
  uint64_t usesFromRegs = 0;
  for (int i = 0; i < regTypeCount; ++i) {
    for (int j = 0; j < regFiles[i].GetRegCnt(); ++j) {
      const auto &myReg = regFiles[i].GetReg(j);
      if (myReg->GetDefCnt() != myReg->GetSizeOfDefList()) {
        Logger::Error(
            "Dag %s: Register Type %d Num %d: New def count %d doesn't match "
            "old def count %d!",
            dagID_, i, j, myReg->GetSizeOfDefList(), myReg->GetDefCnt());
      }
      if (myReg->GetUseCnt() != myReg->GetSizeOfUseList()) {
        Logger::Error(
            "Dag %s: Register Type %d Num %d: New def count %d doesn't match "
            "old def count %d!",
            dagID_, i, j, myReg->GetSizeOfUseList(), myReg->GetUseCnt());
      }
      defsFromRegs += myReg->GetDefCnt();
      usesFromRegs += myReg->GetUseCnt();
    }
  }
  uint64_t defsFromInsts = 0ull;
  uint64_t usesFromInsts = 0ull;
  for (int k = 0; k < instCnt_; ++k) {
    const auto &instruction = insts_[k];
    // Dummy pointer; all that matters here is the length of the uses and defs
    // arrays for each instruction.
    Register **dummy;
    defsFromInsts += instruction->GetDefs(dummy);
    usesFromInsts += instruction->GetUses(dummy);
  }
  bool different = false;
  if (defsFromInsts != defsFromRegs) {
    different = true;
    Logger::Error("Dag %s: Total def count from instructions (%llu) doesn't "
                  "match total def count from registers (%llu)!",
                  dagID_, defsFromInsts, defsFromRegs);
  }
  if (usesFromInsts != usesFromRegs) {
    different = true;
    Logger::Error("Dag %s: Total use count from instructions (%llu) doesn't "
                  "match total use count from registers (%llu)!",
                  dagID_, usesFromInsts, usesFromRegs);
  }
  if (different) {
    Logger::Fatal("Encountered fatal error. Exiting.");
  }
#endif
}

int LLVMDataDepGraph::GetRegisterWeight_(const unsigned resNo) const {
  PSetIterator PSetI = schedDag_->MRI.getPressureSets(resNo);
  return PSetI.getWeight();
}

// A register type is an int value that corresponds to a register type in our
// scheduler.
// We assign multiple register types to each register class in LLVM to account
// for all
// register sets associated with the class.
std::vector<int>
LLVMDataDepGraph::GetRegisterType_(const unsigned resNo) const {
  const TargetRegisterInfo &TRI = *schedDag_->TRI;
  std::vector<int> pSetTypes;

  // if (TRI.isPhysicalRegister(resNo))
  // return pSetTypes;

  PSetIterator PSetI = schedDag_->MRI.getPressureSets(resNo);
  for (; PSetI.isValid(); ++PSetI) {
    const char *pSetName = TRI.getRegPressureSetName(*PSetI);
    int type = llvmMachMdl_->GetRegTypeByName(pSetName);

    pSetTypes.push_back(type);
#ifdef IS_DEBUG_REG_TYPES
    Logger::Info("Pset is %s", pSetName);
#endif
  }

  return pSetTypes;
}

SUnit *LLVMDataDepGraph::GetSUnit(size_t index) const {
  if (index < llvmNodes_.size()) {
    return &llvmNodes_[index];
  } else {
    // Artificial entry/exit node.
    return NULL;
  }
}

// Check if this is a root sunit
bool LLVMDataDepGraph::isRootNode(const SUnit &unit) {
  for (SUnit::const_pred_iterator I = unit.Preds.begin(), E = unit.Preds.end();
       I != E; ++I) {
    if (I->getSUnit()->isBoundaryNode())
      continue;
    else
      return false;
  }
  return true;
}

// Check if this is a leaf sunit
bool LLVMDataDepGraph::isLeafNode(const SUnit &unit) {
  for (SUnit::const_succ_iterator I = unit.Succs.begin(), E = unit.Succs.end();
       I != E; ++I) {
    if (I->getSUnit()->isBoundaryNode())
      continue;
    else
      return false;
  }
  return true;
}

} // end namespace opt_sched
