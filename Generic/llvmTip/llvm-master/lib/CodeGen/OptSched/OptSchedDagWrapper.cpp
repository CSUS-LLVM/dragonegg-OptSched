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

  // TODO(max99x)/(austin): Find real value for this.
  includesUnpipelined_ = true;

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
    #ifdef IS_DEBUG_DAG
    unit.dumpAll(schedDag_);
    #endif
    // Make sure this is a real node
    if(unit.isBoundaryNode() || !unit.isInstr()) continue;

    const MachineInstr* instr = unit.getInstr();

    // Make sure nodes are in numbered order.
    assert(unit.NodeNum == i);
        
    instName = opCode = schedDag_->TII->getName(instr->getOpcode());

    // Search in the machine model for an instType with this OpCode name
    instType = machMdl_->GetInstTypeByName(instName.c_str());

    // If the machine model does not have instType with this OpCode name, use the default instType
    if (instType == INVALID_INST_TYPE)
    {
        #ifdef IS_DEBUG_DAG
        Logger::Info("Instruction %s was not found in machine model. Using the default", 
                     instName.c_str()); 
        #endif
    	  instName = "Default";
        instType = machMdl_->GetInstTypeByName("Default");  
    }

    CreateNode_(unit.NodeNum,
                instName.c_str(),
                instType,
                opCode.c_str(),
                0,  // nodeID
                0, 
                0, 
                0,  // fileInstLwrBound
                0,  // fileInstUprBound
                0);  // blkNum
    if (unit.isCall) includesCall_ = true;
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
    const SUnit& unit = llvmNodes_[i];
    const MachineInstr* instr = unit.getInstr();
    for (SUnit::const_succ_iterator it = unit.Succs.begin();
         it != unit.Succs.end();
         it++) {
      // check if the successor is a boundary node
      if(it->isArtificial() || it->getSUnit()->isBoundaryNode() || !it->getSUnit()->isInstr()) continue;

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
        #ifdef IS_DEBUG_BUILD_DAG
        Logger::Info("Dep type %d with latency %d from Instruction %s",
                     depType, ltncy, instName.c_str()); 
        #endif
      }
      else if(prcsn == LTP_ROUGH) { // use the compiler's rough latency
         ltncy = it->getLatency();
      }
      else
         ltncy = 1;

      CreateEdge_(unit.NodeNum, it->getSUnit()->NodeNum, ltncy, depType);

      #ifdef IS_DEBUG_BUILD_DAG
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
  }
  AdjstFileSchedCycles_();
  PrintEdgeCntPerLtncyInfo();
}

void LLVMDataDepGraph::CountDefs(RegisterFile regFiles[]) {
  std::vector<int> regDefCounts(machMdl_->GetRegTypeCnt());
  for (std::vector<SUnit>::iterator it = llvmNodes_.begin(); 
       it != llvmNodes_.end(); it++) {
    // Vector of registers defined by this node
    std::vector<unsigned> definedRegs;
    for (SUnit::const_succ_iterator I = it->Succs.begin(), E = it->Succs.end();
         I != E; ++I) {
      if (I->isArtificial() || !I->isAssignedRegDep() || !I->getSUnit()->isInstr() || I->getSUnit()->isBoundaryNode()) continue;
      // TODO(austin) clean up
      // make sure this def has not already been found with differnt edge
      unsigned resNo = I->getReg();
      bool regAlreadyFound = false;
      for (int i = 0; i < definedRegs.size(); ++i) {
        if(resNo == definedRegs[i]) {
          regAlreadyFound = true; 
          break;
        }
      }
      if(regAlreadyFound) continue;

      definedRegs.push_back(resNo);

     int regType = GetRegisterType_(resNo);
     // Skip non-register results.
     if (regType == INVALID_VALUE) continue;
     regDefCounts[regType]++;

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
  }
}

void LLVMDataDepGraph::AddDefsAndUses(RegisterFile regFiles[]) {
  // The index of the last "assigned" register for each register type.  
  std::vector<int> regIndices(machMdl_->GetRegTypeCnt());
  // Maps reg number to the Node where the reg was defined and the OptSched Register 
  // associated with the register
  std::map<std::pair<unsigned, unsigned>, Register*> definedRegs;

  // NOTE: We want track physical register definitions even for cases where the
  // two nodes participating in the dependency are in the same SUnit (and
  // therefore always scheduled together), as we need to know when physical
  // registers are clobbered. However, for such cases we act as if the register
  // was never used.
  std::vector<SUnit>::iterator it;
	for (it = llvmNodes_.begin(); 
       it != llvmNodes_.end(); it++) {

    // vector of registers defined by this node
    std::vector<unsigned> definedDefs;
    // Create defs
    for (SUnit::const_succ_iterator I = it->Succs.begin(), E = it->Succs.end();
         I != E; ++I) {

      if (I->isArtificial() || !I->isAssignedRegDep() || !I->getSUnit()->isInstr() || I->getSUnit()->isBoundaryNode()) continue;

			// make sure this def has not already been found with differnt edge
      unsigned resNo = I->getReg();
      bool regAlreadyFound = false;
      for (int i = 0; i < definedDefs.size(); ++i) {
        if(resNo == definedDefs[i]) {
          regAlreadyFound = true;
          break;
        }
      }
      if(regAlreadyFound) continue;
			definedDefs.push_back(resNo);

      bool isPhysReg = schedDag_->TRI->isPhysicalRegister(resNo);
      int regType = GetRegisterType_(resNo);
      // Skip non-register results.
      if (regType == INVALID_VALUE) {
        #ifdef IS_DEBUG_DEFS_AND_USES
        Logger::Info("resNo %lu is not a reg",resNo);
        #endif
        continue;
      }
      Register* reg = regFiles[regType].GetReg(regIndices[regType]++);
      //if (isPhysReg) reg->SetPhysicalNumber(resNo);
      #ifdef IS_DEBUG_DEFS_AND_USES
      if (isPhysReg)
        Logger::Info("Adding Def for physical register: %lu NodeNum: %lu", resNo, it->NodeNum);
      else
        Logger::Info("Adding Def for virtual register: %lu NodeNum: %lu", resNo, it->NodeNum);
      #endif
      insts_[it->NodeNum]->AddDef(reg);
      reg->AddDef();
      definedRegs[std::make_pair(resNo, it->NodeNum)] = reg;
    }

    // Create uses.
    // Find register numbers for uses of this node
    for(SUnit::const_pred_iterator I = it->Preds.begin(), E = it->Preds.end();
        I != E; ++I) {

      // TODO(austin) Only count data edges. Is this right?
			// Confirm that this is a data edge with isCtrl
      if (I->isArtificial() 
          || !I->isAssignedRegDep() 
          || !I->getSUnit()->isInstr() 
          || I->isCtrl()
          || I->getSUnit()->isBoundaryNode()) continue;

      unsigned resNo = I->getReg();
      SUnit* src = I->getSUnit();
      SUnit* dst = &(*it);
      if (definedRegs.find(std::make_pair(resNo, src->NodeNum)) == definedRegs.end()) {
        #ifdef IS_DEBUG_DEFS_AND_USES
        Logger::Info("resNo %lu is used but Not found", resNo);
        #endif
        continue;
      }
      Register* reg = definedRegs.find(std::make_pair(resNo, src->NodeNum))->second;
      if (src->NodeNum == it->NodeNum) continue;
    	// Finally add the use.
    	if (!insts_[it->NodeNum]->FindUse(reg)) {
        #ifdef IS_DEBUG_DEFS_AND_USES
        Logger::Info("Adding use for virtual register: %lu NodeNum: %lu", resNo, it->NodeNum);
        #endif
      	insts_[it->NodeNum]->AddUse(reg);
      	reg->AddUse();
    	}
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

  #ifdef IS_DEBUG_OUT_EDGES
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
          #ifdef IS_DEBUG_OUTEDGES
            Logger::Info("Output edge from [%p] %s to [%p] %s on register %d",
                         P2, P2->GetOpCode(), P1, P1->GetOpCode(), R);
          #endif
          CreateEdge(P2, P1, 0, DEP_OUTPUT);

          #ifdef IS_DEBUG_OUTEDGES
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

// Check if this is a root sunit
bool LLVMDataDepGraph::isRootNode(const SUnit& unit) {
  for(SUnit::const_pred_iterator I = unit.Preds.begin(), E = unit.Preds.end();
             I != E; ++I) {
  	if(I->isArtificial() || !I->getSUnit()->isInstr() || I->getSUnit()->isBoundaryNode()) continue;  
		else return false;
  }
	return true;
}

// Check if this is a leaf sunit
bool LLVMDataDepGraph::isLeafNode(const SUnit& unit) {
  for(SUnit::const_succ_iterator I = unit.Succs.begin(), E = unit.Succs.end();
             I != E; ++I) {
    if(I->isArtificial() || !I->getSUnit()->isInstr() || I->getSUnit()->isBoundaryNode()) continue;
    else return false;
  }
  return true;
}

} // end namespace opt_sched
