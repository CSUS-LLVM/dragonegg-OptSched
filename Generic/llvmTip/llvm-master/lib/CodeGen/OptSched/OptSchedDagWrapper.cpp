/*******************************************************************************
Description:  A wrapper that convert an LLVM ScheduleDAG to an OptSched
              DataDepGraph.
Author:       Max Shawabkeh
Created:      Apr. 2011
Last Update:  Mar. 2017
*******************************************************************************/

#include "llvm/CodeGen/OptSched/OptSchedDagWrapper.h"
#include <cstdio>
#include <queue>
#include <set>
#include <map>
#include <vector>
#include <stack>
#include "llvm/IR/Function.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/RegisterPressure.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/OptSched/generic/logger.h"
#include "llvm/CodeGen/OptSched/basic/register.h"

#define DEBUG_TYPE "optsched"

namespace opt_sched {

using namespace llvm;

LLVMDataDepGraph::LLVMDataDepGraph(MachineSchedContext* context,
                                   ScheduleDAGMILive* llvmDag,
                                   LLVMMachineModel* machMdl,
                                   LATENCY_PRECISION ltncyPrcsn,
                                   bool treatOrderDepsAsDataDeps,
                                   int maxDagSizeForPrcisLtncy)
    : DataDepGraph(machMdl, ltncyPrcsn),
      llvmNodes_(llvmDag->SUnits),
      context_(context),
      schedDag_(llvmDag),
      target_(llvmDag->TM) {
  llvmMachMdl_ = static_cast<LLVMMachineModel*>(machMdl_);
  dagFileFormat_ = DFF_BB;
  isTraceFormat_ = false;
  ltncyPrcsn_ = ltncyPrcsn;
  treatOrderDepsAsDataDeps_ = treatOrderDepsAsDataDeps;
  maxDagSizeForPrcisLtncy_ = maxDagSizeForPrcisLtncy;

  // The extra 2 are for the artifical root and leaf nodes.
  instCnt_ = nodeCnt_ = llvmNodes_.size() + 2;

  // TODO(max99x): Find real weight.
  weight_ = 1.0f;

  std::snprintf(dagID_, MAX_NAMESIZE, "%s:%s",
                context_->MF->getFunction()->getName().data(),
                context_->MF->front().getBasicBlock()->getName().data());

  std::snprintf(compiler_, MAX_NAMESIZE, "LLVM");

  AllocArrays_(instCnt_);

  includesNonStandardBlock_ = false;
  includesUnsupported_ = false;

  // TODO(max99x): Find real value for this.
  includesUnpipelined_ = false;

  ConvertLLVMNodes_();
      
  if (Finish_() == RES_ERROR) Logger::Fatal("DAG Finish_() failed.");
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
    const SUnit& unit = llvmNodes_[i];
    const MachineInstr* instr = unit.getInstr();

    // Make sure nodes are in numbered order.
    assert(unit.NodeNum == i);
        
    instName = opCode = schedDag_->TII->getName(instr->getOpcode());

    // Search in the machine model for an instType with this OpCode name
    instType = machMdl_->GetInstTypeByName(instName.c_str());

    // If the machine model does not have instType with this OpCode name, use the default instType
    if (instType == INVALID_INST_TYPE)
    {
        Logger::Info("Instruction %s was not found in machine model. Using the default", 
                     instName.c_str()); 
    	  instName = "Default";
        instType = machMdl_->GetInstTypeByName("Default");  
    }

    CreateNode_(unit.NodeNum,
                instName.c_str(),
                instType,
                opCode.c_str(),
                0,  // nodeID
                unit.NodeQueueId + 1, 
                unit.NodeQueueId + 1, 
                0,  // fileInstLwrBound
                0,  // fileInstUprBound
                0);  // blkNum
    if (unit.isCall) includesCall_ = true;
    if (unit.NumPreds == 0) roots.push_back(unit.NodeNum);
    if (unit.NumSuccs == 0) leaves.push_back(unit.NodeNum);

    DEBUG(dbgs() << "Creating Node: " << unit.NodeNum << " " <<
          schedDag_->TII->getName(instr->getOpcode()) << "\n");
  }

  // Create edges.
  for (size_t i = 0; i < llvmNodes_.size(); i++) {
    const SUnit& unit = llvmNodes_[i];
    const MachineInstr* instr = unit.getInstr();
    for (SUnit::const_succ_iterator it = unit.Succs.begin();
         it != unit.Succs.end();
         it++) {
      // check if the successor is a boundary node
      if(it->getSUnit()->isBoundaryNode()) continue;

      DependenceType depType;
      switch (it->getKind()) {
        case SDep::Data:   depType = DEP_DATA; break;
        case SDep::Anti:   depType = DEP_ANTI; break;
        case SDep::Output: depType = DEP_OUTPUT; break;
        case SDep::Order:  depType = treatOrderDepsAsDataDeps_? DEP_DATA : DEP_OTHER; break;
      }

      LATENCY_PRECISION prcsn = ltncyPrcsn_;
      if (prcsn == LTP_PRECISE && maxDagSizeForPrcisLtncy_ >0 && llvmNodes_.size() > maxDagSizeForPrcisLtncy_) 
        prcsn = LTP_ROUGH; // use rough latencies if DAG is too large 

      if(prcsn == LTP_PRECISE) { // if precise latency, get the precise latency from the machine model
        instName = schedDag_->TII->getName(instr->getOpcode());
	      instType = machMdl_->GetInstTypeByName(instName);
        ltncy = machMdl_->GetLatency(instType, depType);
        Logger::Info("Dep type %d with latency %d from Instruction %s", depType, ltncy, instName.c_str()); 
      }
      else if(prcsn == LTP_ROUGH) { // use the compiler's rough latency
         ltncy = it->getLatency();
      }
      else
         ltncy = 1;

      CreateEdge_(unit.NodeNum, it->getSUnit()->NodeNum, ltncy, depType);

      #ifdef IS_DEBUG
      Logger::Info("Creating an edge from %d to %d. Type is %d, latency = %d", 
                   unit.NodeNum, it->getSUnit()->NodeNum, depType, ltncy);
      #endif
    }
  }

  size_t maxNodeNum = llvmNodes_.size() - 1;

  // Create artificial root.
  assert(roots.size() > 0 && leaves.size() > 0);
  int rootNum = ++maxNodeNum;
  root_ = CreateNode_(rootNum,
              "artificial",
              machMdl_->GetInstTypeByName("artificial"),
              "__optsched_entry",
              0,  // nodeID
              0,  // fileSchedOrder
              0,  // fileSchedCycle
              0,  // fileInstLwrBound
              0,  // fileInstUprBound
              0);  // blkNum
  for (size_t i = 0; i < roots.size(); i++) {
    CreateEdge_(rootNum, roots[i], 0, DEP_OTHER);
    Logger::Info("Inst %d is a root", roots[i]);
  }

  // Create artificial leaf.
  int leafNum = ++maxNodeNum;
  CreateNode_(leafNum,
              "artificial",
              machMdl_->GetInstTypeByName("artificial"),
              "__optsched_exit",
              0,  // nodeID
              llvmNodes_.size() + 1, 
              llvmNodes_.size() + 1, 
              0,  // fileInstLwrBound
              0,  // fileInstUprBound
              0);  // blkNum
  for (size_t i = 0; i < leaves.size(); i++) {
    CreateEdge_(leaves[i], leafNum, 0, DEP_OTHER);
    Logger::Info("Inst %d is a leaf", leaves[i]);
  }
  AdjstFileSchedCycles_();
  PrintEdgeCntPerLtncyInfo();
}

void LLVMDataDepGraph::CountDefs(RegisterFile regFiles[]) {
  std::vector<int> regDefCounts(machMdl_->GetRegTypeCnt());
  MachineInstr* instr;
  for (std::vector<SUnit>::iterator it = llvmNodes_.begin(); 
       it != llvmNodes_.end(); it++) {
    instr = it->getInstr();
    // get defs for this MachineInstr
    RegisterOperands ops;
    ops.collect(*instr, *schedDag_->TRI, schedDag_->MRI, false, false);
    for(SmallVector<RegisterMaskPair, 8>::iterator regIt = ops.Defs.begin();
        regIt != ops.Defs.end(); regIt++) {
      int regType = GetRegisterType_(regIt->RegUnit);
      // Skip non-register results.
      if (regType == INVALID_VALUE) continue;
      regDefCounts[regType]++;
    } 
		for(SmallVector<RegisterMaskPair, 8>::iterator regIt = ops.DeadDefs.begin();
        regIt != ops.DeadDefs.end(); regIt++) {
      int regType = GetRegisterType_(regIt->RegUnit);
      // Skip non-register results.
      if (regType == INVALID_VALUE) continue;
      regDefCounts[regType]++;
    }
  }

  for (int i = 0; i < machMdl_->GetRegTypeCnt(); i++) {
    #ifdef IS_DEBUG
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
  // Maps node,resultNumber pairs (virtual registers) to opt_sched registers.
  std::map<std::pair<const MachineInstr*, unsigned>, Register*> definedRegs;

  // NOTE: We want track physical register definitions even for cases where the
  // two nodes participating in the dependency are in the same SUnit (and
  // therefore always scheduled together), as we need to know when physical
  // registers are clobbered. However, for such cases we act as if the register
  // was never used.
  MachineInstr* instr;
  std::vector<SUnit>::iterator it;
	for (it = llvmNodes_.begin(); 
       it != llvmNodes_.end(); it++) {
    // Skip nodes excluded from ScheduleDAG.
    if (it->isBoundaryNode()) continue;
    instr = it->getInstr();
    // get defs for this MachineInstr
    RegisterOperands ops;
    ops.collect(*instr, *schedDag_->TRI, schedDag_->MRI, false, false);
    int numDefs = ops.Defs.size() + ops.DeadDefs.size();

    #ifdef IS_DEBUG
    Logger::Info("Inst %d %s has %d defs", 
                 it->NodeNum, insts_[it->NodeNum]->GetOpCode(), numDefs);
    #endif  
 
    // Create defs.
		for(SmallVector<RegisterMaskPair, 8>::iterator regIt = ops.Defs.begin();
        regIt != ops.Defs.end(); regIt++) {
      unsigned resNo = regIt->RegUnit;
      bool isPhysReg = schedDag_->TRI->isPhysicalRegister(resNo);
      int regType = GetRegisterType_(resNo);
      // Skip non-register results.
      if (regType == INVALID_VALUE) { 
        #ifdef IS_DEBUG
        Logger::Info("resNo %d is not a reg",resNo);
        #endif 
        continue;
			}
			 Register* reg = regFiles[regType].GetReg(regIndices[regType]++);
       if (isPhysReg) reg->SetPhysicalNumber(resNo); 
       dbgs() << "Adding def " << it->NodeNum << ":" << resNo << "\n";
       insts_[it->NodeNum]->AddDef(reg);
       reg->AddDef();
       definedRegs[std::make_pair(instr, resNo)] = reg;
    }

			// TODO(austin) fix this ugly mess
		for(SmallVector<RegisterMaskPair, 8>::iterator regIt = ops.DeadDefs.begin();
        regIt != ops.DeadDefs.end(); regIt++) {
      unsigned resNo = regIt->RegUnit;
      bool isPhysReg = schedDag_->TRI->isPhysicalRegister(resNo);
      int regType = GetRegisterType_(resNo);
      // Skip non-register results.
      if (regType == INVALID_VALUE) {
        #ifdef IS_DEBUG
        Logger::Info("resNo %d is not a reg",resNo);
        #endif
        continue;
			}
			Register* reg = regFiles[regType].GetReg(regIndices[regType]++);
      if (isPhysReg) reg->SetPhysicalNumber(resNo); 
      dbgs() << "Adding def " << it->NodeNum << ":" << resNo << "\n";
      insts_[it->NodeNum]->AddDef(reg);
      reg->AddDef();
      definedRegs[std::make_pair(instr, resNo)] = reg;
    }

    // Create uses.
		for(SmallVector<RegisterMaskPair, 8>::iterator regIt = ops.Uses.begin();
        regIt != ops.Uses.end(); regIt++) {
      unsigned resNo = regIt->RegUnit;

      if (definedRegs.find(std::make_pair(instr, resNo)) == definedRegs.end()) {
        #ifdef IS_DEBUG
        Logger::Info("resNo %d is used but Not found", resNo);
        #endif
        continue;
      }
      Register* reg = definedRegs.find(std::make_pair(instr, resNo))->second;

      //TODO(austin) implement
      /*
      // Ignore non-data edges.
      bool isDataEdge = true;
      for (SUnit::const_succ_iterator succIt = it.Succs.begin();
           succIt != it.Succs.end();
           it++) {
        if (succIt->setSUnit()->NodeNum == (unsigned)dst->getNodeId() &&
            succIt->getKind() != SDep::Data) {
          isDataEdge = false;
        }
      }
      if (!isDataEdge) continue;
      */

      // Finally add the use.
      //if (src->getNodeId() != dst->getNodeId()) {
      if (!insts_[it->NodeNum]->FindUse(reg)) {
      	dbgs() << "Adding use " << it->NodeNum << ":" << resNo << "\n";
        insts_[it->NodeNum]->AddUse(reg);
        reg->AddUse();
      }
      //}
    }
  }
}

void LLVMDataDepGraph::AddOutputEdges() {
  // Maps physical register number to their live ranges.
  std::map<int, std::vector<LiveRange> > ranges;
  // Maps each {physical register number and user node} to their live range.
  std::map<std::pair<int, SchedInstruction*>, SchedInstruction*> sources;

  // Find live ranges.
  for (int i = 0; i < GetInstCnt(); i++) {
    SchedInstruction* inst = GetInstByIndx(i);
    Register** defs;
    int defsCount = inst->GetDefs(defs);
    for (int j = 0; j < defsCount; j++) {
      Register* reg = defs[j];
      if (reg->IsPhysical()) {
        int regNumber = reg->GetPhysicalNumber();
        ranges[regNumber].push_back(LiveRange());
        LiveRange& range = ranges[regNumber].back();
        range.producer = inst;
        for (SchedInstruction* succ = inst->GetFrstScsr();
             succ != NULL;
             succ = inst->GetNxtScsr()) {
          if (succ->FindUse(reg)) {
            range.consumers.push_back(succ);
            sources[std::make_pair(regNumber, succ)] = inst;
          }
        }
      }
    }
  }

  #ifdef IS_DEBUG
    for (std::map<int, vector<LiveRange> >::iterator it = ranges.begin();
         it != ranges.end();
         it++) {
      Logger::Info("Ranges for %d:", it->first);
      for (unsigned i = 0; i < it->second.size(); i++) {
        Logger::Info("  Producer [%p] %s",
                     it->second[i].producer, 
                     it->second[i].producer->GetOpCode());
        for (unsigned j = 0; j < it->second[i].consumers.size(); j++) {
          Logger::Info("    Consumer [%p] %s",
                       it->second[i].consumers[j], 
                       it->second[i].consumers[j]->GetOpCode());
        }
      }
    }
  #endif

  // Create the output edges.
  std::map<std::pair<int, SchedInstruction*>, SchedInstruction*>::iterator it;
  for (it = sources.begin(); it != sources.end(); it++) {
    int R = it->first.first;
    SchedInstruction* U = it->first.second;
    SchedInstruction* P1 = it->second;
    vector<LiveRange>& regRanges = ranges[R];
    std::stack<SchedInstruction*> ancestors;
    std::set<SchedInstruction*> seen;

    for (SchedInstruction* predecessor = U->GetFrstPrdcsr();
         predecessor != NULL;
         predecessor = U->GetNxtPrdcsr()) {
      ancestors.push(predecessor);
      seen.insert(predecessor);
    }

    while (!ancestors.empty()) {
      SchedInstruction* P2 = ancestors.top();
      ancestors.pop();
      if (P1 == P2) continue;
      bool foundConflict = false;
      for (unsigned i = 0; i < regRanges.size(); i++) {
        if (regRanges[i].producer == P2) {
          // TODO(max): Get output dependency latency from machine model.
          #ifdef IS_DEBUG
            Logger::Info("Output edge from [%p] %s to [%p] %s on register %d",
                         P2, P2->GetOpCode(), P1, P1->GetOpCode(), R);
          #endif
          CreateEdge(P2, P1, 0, DEP_OUTPUT);

          #ifdef IS_DEBUG_DAG
          Logger::Info("Creating an output edge from %d to %d", P2->GetNum(), P1->GetNum());
          #endif

          foundConflict = true;
          break;
        }
      }

      if (!foundConflict) {
        for (SchedInstruction* predecessor = P2->GetFrstPrdcsr();
             predecessor != NULL;
             predecessor = P2->GetNxtPrdcsr()) {
          if (seen.find(predecessor) != seen.end()) continue;
          ancestors.push(predecessor);
          seen.insert(predecessor);
        }
      }
    }
  }
}

int LLVMDataDepGraph::GetRegisterType_(const unsigned resNo) const {
  const TargetRegisterInfo& TRI = *schedDag_->TRI;
  const TargetRegisterClass* regClass;
  // Check if is a physical register
  if (schedDag_->TRI->isPhysicalRegister(resNo)) {
    regClass = TRI.getMinimalPhysRegClass(resNo);
    if (regClass == NULL) return INVALID_VALUE;
    return llvmMachMdl_->GetRegType(regClass, &TRI);
  } 
  
  else if(schedDag_->TRI->isVirtualRegister(resNo)) {
    regClass = schedDag_->MRI.getRegClass(resNo);
    if (regClass == NULL) return INVALID_VALUE;
    return llvmMachMdl_->GetRegType(regClass, &TRI);
  }

  else {
    return INVALID_VALUE;
  }
}

SUnit* LLVMDataDepGraph::GetSUnit(size_t index) const {
  if (index < llvmNodes_.size()) {
    return &llvmNodes_[index];
  } else {
    // Artificial entry/exit node.
    return NULL;
  }
}

} // end namespace opt_sched
