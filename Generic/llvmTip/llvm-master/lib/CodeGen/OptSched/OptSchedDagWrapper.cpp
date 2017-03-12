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
#include "llvm/CodeGen/FunctionLoweringInfo.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/OptSched/generic/logger.h"
#include "llvm/CodeGen/OptSched/basic/register.h"

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
      target_(context->PassConfig->template getTM<TargetMachine>()) {
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
                context_->FuncInfo->MBB->getBasicBlock()->getName().data());

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

  #ifdef IS_DEBUG_DAG
  Logger::Info("Building opt_sched DAG out of llvm DAG"); 
  #endif

  // Create nodes.
  for (size_t i = 0; i < llvmNodes_.size(); i++) {
    const SUnit& unit = llvmNodes_[i];
    const SDNode* node = unit.getNode()->getGluedMachineNode();

    // Make sure nodes are in numbered order.
    assert(unit.NodeNum == i);
        
    instName = opCode = node->getOperationName();

    // Search in the machine model for an instType with this OpCode name
    instType = machMdl_->GetInstTypeByName(instName.c_str());

    // If the machine model does not have instType with this OpCode name, use the default instType
    if (instType == INVALID_INST_TYPE)
    {
//        Logger::Info("Instruction %s was not found in machine model. Using the default", instName.c_str()); 
    	instName = "Default";
        instType = machMdl_->GetInstTypeByName("Default");  
    }
//    else
//        Logger::Info("Instruction %s was found in machine model with latency %d", instName.c_str(), machMdl_->GetLatency(instType, DEP_DATA)); 

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

    #ifdef IS_DEBUG_DAG 
    Logger::Info("Creating Node %d: %s", unit.NodeNum, node->getOperationName().c_str());
    #endif
  }


  // Create edges.
  for (size_t i = 0; i < llvmNodes_.size(); i++) {
    const SUnit& unit = llvmNodes_[i];
    const SDNode* node = unit.getNode()->getGluedMachineNode();
    for (SUnit::const_succ_iterator it = unit.Succs.begin();
         it != unit.Succs.end();
         it++) {
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
        instName = node->getOperationName();
	instType = machMdl_->GetInstTypeByName(instName.c_str());
        ltncy = machMdl_->GetLatency(instType, depType);
//        Logger::Info("Dep type %d with latency %d from Instruction %s", depType, ltncy, instName.c_str()); 
      }
      else if(prcsn == LTP_ROUGH) { // use the compiler's rough latency
         ltncy = it->getLatency();
      }
      else
         ltncy = 1;

      CreateEdge_(unit.NodeNum, it->getSUnit()->NodeNum, ltncy, depType);

      #ifdef IS_DEBUG_DAG
      Logger::Info("Creating an edge from %d to %d. Type is %d, latency = %d", 
                   unit.NodeNum, it->getSUnit()->NodeNum, depType, ltncy);
      #endif

      #ifdef IS_DEBUG_LLVM_SDOPT
        Logger::Info("%d %s -> %d %s",
                     unit.NodeNum,
                     unit.getNode()->getGluedMachineNode()->
                        getOperationName().c_str(),
                     it->getSUnit()->NodeNum,
                     it->getSUnit()->getNode()->getGluedMachineNode()->
                        getOperationName().c_str());
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
//Logger::Info("Inst %d is a root", roots[i]);
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
//Logger::Info("Inst %d is a leaf", leaves[i]);
  }
  AdjstFileSchedCycles_();
  PrintEdgeCntPerLtncyInfo();
}

void LLVMDataDepGraph::CountDefs(RegisterFile regFiles[]) {
  std::vector<int> regDefCounts(machMdl_->GetRegTypeCnt());
  SDNode node;
  for (std::vector<SUnit>::iterator it = llvmNodes_.begin(); 
       it != llvmNodes.end(); it++) {
    node = it->getNode();
    // Skip nodes excluded from ScheduleDAG.
    if (node->getNodeId() == -1) continue;

    for (unsigned resNo = 0; resNo < node->getNumValues(); resNo++) {
      int regType = GetRegisterType_(node, resNo);
      // Skip non-register results.
      if (regType == INVALID_VALUE) continue;
      regDefCounts[regType]++;
    }
  }

  for (int i = 0; i < machMdl_->GetRegTypeCnt(); i++) {
    #ifdef IS_DEBUG_LLVM_SDOPT
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
  std::map<std::pair<const SDNode*, unsigned>, Register*> definedRegs;

  // NOTE: We want track physical register definitions even for cases where the
  // two nodes participating in the dependency are in the same SUnit (and
  // therefore always scheduled together), as we need to know when physical
  // registers are clobbered. However, for such cases we act as if the register
  // was never used.
  SDNode node;
	for (std::vector<SUnit>::iterator it = llvmNodes_.begin(); 
       it != llvmNodes.end(); it++) {
    node = it->getNode();
    // Skip nodes excluded from ScheduleDAG.
    if (node->getNodeId() == -1) continue;

    #ifdef IS_DEBUG_DEFS_AND_USES
    Logger::Info("\nInst %d %s has %d defs", 
                 node->getNodeId(), insts_[node->getNodeId()]->GetOpCode(), node->getNumValues());
    #endif  
 
    // Create defs.
    for (unsigned resNo = 0; resNo < node->getNumValues(); resNo++) {
      unsigned physReg = GetPhysicalRegister_(node, resNo);
      int regType = GetRegisterType_(node, resNo);
      // Skip non-register results.
      if (regType == INVALID_VALUE) { 
        #ifdef IS_DEBUG_DEFS_AND_USES
        Logger::Info("resNo %d is not a reg",resNo);
        #endif 
        continue;
      }

      Register* reg = regFiles[regType].GetReg(regIndices[regType]++);
      if (physReg) reg->SetPhysicalNumber(physReg);
      insts_[node->getNodeId()]->AddDef(reg);
      reg->AddDef();

      #ifdef IS_DEBUG_DEFS_AND_USES
      Logger::Info("Inst %d %s defines resNo %d reg %d of type %d (%s). Phys = %s", 
                   node->getNodeId(), insts_[node->getNodeId()]->GetOpCode(), resNo, reg->GetNum(), regType,  
                   llvmMachMdl_->GetRegTypeName(regType).c_str(), physReg? "YES":"NO");
      #endif

      definedRegs[std::make_pair(node, resNo)] = reg;

      #ifdef IS_DEBUG_LLVM_SDOPT
        Logger::Info("DEF %s (%s) by %s:%d (%s)",
                     physReg ? target_.getRegisterInfo()->getName(physReg)
                             : "virtual",
                     llvmMachMdl_->GetRegTypeName(regType).c_str(),
                     node->getOperationName().c_str(),
                     resNo,
                     llvmNodes_[node->getNodeId()].getNode()->
                         getGluedMachineNode()->
                         getOperationName().c_str());
      #endif
    }

    // Create uses.
    SDNode::use_iterator it;
    for (it = node->use_begin(); it != node->use_end(); it++) {
      unsigned resNo = it.getUse().getResNo();
      const SDNode* src = node;
      const SDNode* dst = *it;

      if (definedRegs.find(std::make_pair(node, resNo)) == definedRegs.end()) {
        #ifdef IS_DEBUG_DEFS_AND_USES
        Logger::Info("resNo %d is used but Not found", resNo);
        #endif
        continue;
      }
      Register* reg = definedRegs.find(std::make_pair(node, resNo))->second;

      // Skip nodes excluded from ScheduleDAG.
      if (dst->getNodeId() < 0) continue;

      // Ignore non-data edges.
      const SUnit& srcUnit = llvmNodes_[src->getNodeId()];
      bool isDataEdge = true;
      for (SUnit::const_succ_iterator it = srcUnit.Succs.begin();
           it != srcUnit.Succs.end();
           it++) {
        if (it->getSUnit()->NodeNum == (unsigned)dst->getNodeId() &&
            it->getKind() != SDep::Data) {
          isDataEdge = false;
        }
      }
      if (!isDataEdge) continue;

      // Finally add the use.
      if (src->getNodeId() != dst->getNodeId()) {
        #ifdef IS_DEBUG_LLVM_SDOPT
          Logger::Info("  USE %s:%d (%s) by %s (%s)",
                       src->getOperationName().c_str(),
                       resNo,
                       llvmNodes_[src->getNodeId()].getNode()->
                           getGluedMachineNode()->
                           getOperationName().c_str(),
                       dst->getOperationName().c_str(),
                       llvmNodes_[dst->getNodeId()].getNode()->
                           getGluedMachineNode()->
                           getOperationName().c_str());
        #endif
      int regType = reg->GetType();
        if (!insts_[dst->getNodeId()]->FindUse(reg)) {
          insts_[dst->getNodeId()]->AddUse(reg);
          reg->AddUse();
          #ifdef IS_DEBUG_DEFS_AND_USES
          Logger::Info("Inst %d %s uses resNo %d reg %d of type %d (%s)",   
                       dst->getNodeId(), insts_[dst->getNodeId()]->GetOpCode(), resNo, reg->GetNum(), regType,  
                       llvmMachMdl_->GetRegTypeName(regType).c_str());
          #endif
        }
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

  #ifdef IS_DEBUG_OUTPUT_EDGES
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
          #ifdef IS_DEBUG_OUTPUT_EDGES
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

unsigned LLVMDataDepGraph::GetPhysicalRegister_(const SDNode* node,
                                                const unsigned resNo) const {
  const TargetRegisterInfo* TRI = target_.getRegisterInfo();
  if (node->isMachineOpcode()) {
    const MCInstrDesc &II = schedDag_->TII->get(node->getMachineOpcode());
    if (II.ImplicitDefs && resNo >= II.getNumDefs()) {
      unsigned physReg = II.ImplicitDefs[resNo - II.getNumDefs()];
      if (!physReg) return 0;  // Not a real physical register.
      // Always consider the largest super-register.
      // NOTE: Assumes the super-register relation is transitive.
      // <Najem> TODO::: FIX THIS
      //while (unsigned super = *TRI.getSuperReg()) physReg = super;
      //while (unsigned super = *TRI.getRegister(physReg)) physReg = super;
//      for (MCSuperRegIterator Supers(physReg, TRI); Supers.isValid(); ++Supers)
//    	  physReg = *Supers;

      MCSuperRegIterator* Super = new MCSuperRegIterator(physReg, TRI);
      while(Super->isValid())
      {
    	  physReg = **Super;
    	  delete Super;
    	  Super = new MCSuperRegIterator(physReg, TRI);
      }

      delete Super;

      return physReg;
    }
  }
  return 0;
}

int LLVMDataDepGraph::GetPhysicalRegType_(unsigned reg) const {
  const TargetRegisterInfo& TRI = *target_.getRegisterInfo();

  // Find the first type that contains this register.
  for (int i = 0; i < llvmMachMdl_->GetRegTypeCnt(); i++) {
    const TargetRegisterClass* regClass = llvmMachMdl_->GetRegClass(i);
    if (regClass->contains(reg)) {
      // HACK: On x86-64, GR32 is mapped to GR64.
      if (llvmMachMdl_->GetModelName() == "x86-64" &&
          llvmMachMdl_->GetRegTypeName(i) == "GR32") {
        i = llvmMachMdl_->GetRegTypeByName("GR64");
      }
      return i;
    }
  }

  Logger::Fatal("No type for physical register %s found.", TRI.getName(reg));

  return INVALID_VALUE;
}

int LLVMDataDepGraph::GetRegisterType_(const SDNode* node,
                                       const unsigned resNo) const {
  if (unsigned reg = GetPhysicalRegister_(node, resNo)) {
    return GetPhysicalRegType_(reg);
  } else {
    const TargetRegisterClass* regClass;
    regClass = context_->TLI.getRepRegClassFor(node->getSimpleValueType(resNo));
    if (regClass == NULL) return INVALID_VALUE;
    return llvmMachMdl_->GetRegType(regClass);
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
