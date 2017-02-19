#ifndef LUCCPMACHINESTRATEGY_GAURD
#define LUCCPMACHINESTRATEGY_GAURD

#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/CodeGen/LiveIntervalAnalysis.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/RegisterClassInfo.h"
#include "llvm/CodeGen/ScheduleDFS.h"
#include "llvm/CodeGen/ScheduleHazardRecognizer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetInstrInfo.h"
#include <functional>
#include <queue>
#include <vector>
#include <stdio.h>


using namespace llvm;


struct LUCPath
{
	bool operator()(SUnit *s1, SUnit *s2)
	{
		return s1->LUC < s2->LUC;
	}
};

struct CPPath
{
	bool operator()(SUnit *s1, SUnit *s2)
	{
		return s1->getDepth() < s2->getDepth();
	}
};


struct getReadyNodes
{
	bool operator()(SUnit *s1, SUnit *s2)
	{
		return s1->NumSuccsLeft > s2->NumSuccsLeft;
	}
};

namespace llvm {

	class MachineSchedStrategy;
	class ScheduleDAGMILive;
	/// LUC-CP Scheduler.
	class LUCCPMachineStrategy : public MachineSchedStrategy {

	  ScheduleDAGMILive *DAG;

	  std::priority_queue<SUnit*, std::vector<SUnit*>, CPPath> CP_PQ;

	  std::priority_queue<SUnit*, std::vector<SUnit*>, LUCPath> LUC_PQ;
	  
	  // ReadyQ is populated with all the SUnits from the Dag on construction This is done from class hiearchy
	  std::priority_queue<SUnit*, std::vector<SUnit*>, getReadyNodes> ReadyQ;

	public:
	  LUCCPMachineStrategy(const MachineSchedContext *C): DAG(nullptr){}

	  void initialize(ScheduleDAGMI *dag) override;

	  /// Callback to select the highest priority node from the ready Q.
	  SUnit *pickNode(bool &IsTopNode) override;

	  /// Callback after a node is scheduled. Mark a newly scheduled tree, notify DFSResults, and resort the priority Q.
	  void schedNode(SUnit *SU, bool IsTopNode) override;
	  
	  void releaseTopNode(SUnit *SU) override;

	  void releaseBottomNode(SUnit *SU) override;
	  
	};
}
/*
enum Kind {
      Data,        ///< Regular data dependence (aka true-dependence).
      Anti,        ///< A register anti-dependedence (aka WAR).
      Output,      ///< A register output-dependence (aka WAW).
      Order        ///< Any other ordering dependency.
    };
*/
void LUCCPMachineStrategy::initialize(ScheduleDAGMI *dag){
	printf("LUCPScheduler has been initialized\n");
	DAG = static_cast<ScheduleDAGMILive*>(dag);
}

/// Callback to select the highest priority node from the ready Q.
SUnit* LUCCPMachineStrategy::pickNode(bool &IsTopNode){
	printf("LUCCPMachineStrategy is picking a node\n");
	if (ReadyQ.empty()) return nullptr;
//	printf("Top of pickNode function\n\n\n");
	
	// select nodes that are ready to be scheduled and populate LUC PQ
//	printf("Populating LUC Que\n");
	while(!ReadyQ.empty()){
		
		// get the node with the least preds left
		SUnit *next = ReadyQ.top();
		
//		printf("node: %d\n", next->NodeNum);
		
		if(next->TrueDependencyCountLeft < -30){
			// calculate TrueDependencies
			int tmp = 0;
			for (SUnit::const_succ_iterator dependency = next->Preds.begin(), dependencyLast = next->Preds.end();
			dependency != dependencyLast; ++dependency) {
				if(!dependency->isCtrl()){
					tmp++;
				}
			}
			next->TrueDependencyCountLeft = tmp;
//			printf("node: %d TrueDependencyCountLeft: %d\n", next->NodeNum, next->TrueDependencyCountLeft);
		}

		// if there are no preds left then the node is ready to be scheduled
		if(next->isBottomReady()){
			
//			printf("node: %d is Bottom Ready\n", next->NodeNum);
		
			// Calculate FRESH Last Use Count 
			int tempLUC = 0;
			for (SUnit::const_succ_iterator I = next->Succs.begin(), E = next->Succs.end(); I != E; ++I) {
				SUnit *SuccsSU = I->getSUnit();
				//printf("\nnode: %d checking dependency with Parent node: %d \n", next->NodeNum, SuccsSU->NodeNum);
				  if(!(I->isCtrl())){
//					  printf("node: %d is true dependency of Parent node: %d \n", next->NodeNum, SuccsSU->NodeNum);
					  // If the node's parent has one child then increment the childs last use count
//					  printf("Parent node: %d TrueDependencyCountLeft: %d\n", SuccsSU->NodeNum, SuccsSU->TrueDependencyCountLeft);
					  if(SuccsSU->TrueDependencyCountLeft == 1){
//						  printf("node: %d incrementing LUC count\n", next->NodeNum);
						  tempLUC++;
					  }
				  }
			}		
			next->LUC = tempLUC;
			
			// push the node into the LUC Que
			LUC_PQ.push(next);
//			printf("Adding to LUC Que node: %d, LUC: %d\n\n", next->NodeNum, next->LUC);
			// Remove the node from the Ready Que.
			ReadyQ.pop();
		} else {
//			printf("node: %d is not Bottom Ready\n", next->NodeNum);
			continue;
		}
	}

//	printf("\n\nPopulating the CP Que\n");
	// get the top item from the LUC PQ
	SUnit *SU = LUC_PQ.top();

	// remove from LUC PQ
	LUC_PQ.pop();

	// add top unit to CP
	CP_PQ.push(SU);
	
//	printf("Adding to CP Que node: %d CP: %d\n", SU->NodeNum, SU->getDepth());
	
	// check for ties in the LUC PQ and populate the CP PQ with the ties
	while(!LUC_PQ.empty()){
		SUnit *next = LUC_PQ.top();

		// is there a tie?
		if(SU->LUC == next->LUC){
			// It is a tie. add Tie to CP PQ
			CP_PQ.push(next);
			
//			printf("Adding to CP Que node: %d CP: %d\n", next->NodeNum, next->getDepth());
	
			// remove tie from LUC_PQ
			LUC_PQ.pop();
		} else {
			// not a tie
			break;
		}
	}

	// return the first Item from the tie breaker
	SU = CP_PQ.top();

	// Remove the selected nodes from the CP_PQ.
	// Important to do here and not during clean up otherwise node will be added back into the Ready Q
	CP_PQ.pop();

	// Empties the LUC PQ and the CP PQ back into the ReadyQ
	while(!CP_PQ.empty())
	{
		ReadyQ.push(CP_PQ.top());
		CP_PQ.pop();
	}

	// empty the rest of LUC que back into Node Ready Que
	while(!LUC_PQ.empty())
	{
		ReadyQ.push(LUC_PQ.top());
		LUC_PQ.pop();
	}

//	printf("\n");

	//Decrament the selected nodes parent succs count
	 for (SUnit::const_succ_iterator I = SU->Succs.begin(), E = SU->Succs.end(); I != E; ++I) {
		SUnit *SuccsSU = I->getSUnit();			
//		 printf("\nnode: %d checking dependency with Parent node: %d \n", SU->NodeNum, SuccsSU->NodeNum);
		 if(!I->isCtrl()){
//			printf("node: %d is true dependency of Parent node: %d \n", SU->NodeNum, SuccsSU->NodeNum);
//			printf("Parent node: %d TrueDependencyCountLeft: %d\n", SuccsSU->NodeNum, SuccsSU->TrueDependencyCountLeft);
			SuccsSU->TrueDependencyCountLeft--;
//			printf("Parent node: %d TrueDependencyCountLeft: %d\n", SuccsSU->NodeNum, SuccsSU->TrueDependencyCountLeft);
		 }
	 }
	 
	return SU;
}

/// Callback after a node is scheduled. Mark a newly scheduled tree, notify
/// DFSResults, and resort the priority Q.
void LUCCPMachineStrategy::schedNode(SUnit *SU, bool IsTopNode){}
// Add Node to ReadyQ before scheduling begins
void LUCCPMachineStrategy::releaseTopNode(SUnit *SU){ 
	//ReadyQ.push(SU);  
}
// Add Node to ReadyQ before scheduling begins
void LUCCPMachineStrategy::releaseBottomNode(SUnit *SU){
	ReadyQ.push(SU);  
}

#endif


	/* // Remove all non-true dependencies
	for (unsigned su = 0, e = DAG->SUnits.size(); su != e; ++su){
        SUnit node = DAG->SUnits[su];
		
		if(node.NumPredsLeft == 0) continue;
		
		// list of Preds to be removed
		SmallVector<SDep, 4> deathList;
		
		// loop through Pred dependecy
		for (SUnit::const_succ_iterator dependency = node.Preds.begin(), dependencyLast = node.Preds.end();
		dependency != dependencyLast; ++dependency) {
			//SDep->isCtrl is Shorthand for getKind() != SDep::Data where Data == true dependency
			if(dependency->isCtrl()){
				// dependency is not a true dependency add to death list
				deathList.push_back(*dependency);
			}
		}
		
		// Remove dirty links
		for (SmallVectorImpl<SDep>::iterator I = deathList.begin(), E = deathList.end();
        I != E; ++I) {
			 node.removePred(*I);
		}
		
	}
	*/
