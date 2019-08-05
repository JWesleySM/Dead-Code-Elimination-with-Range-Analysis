#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Instructions.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/CFG.h"
#include "RangeAnalysis.h"

#define DEBUG_TYPE "dead-code-elimination-with-range-analysis"

using namespace llvm;
using namespace RangeAnalysis;

STATISTIC(InstructionsEliminated, "Number of instructions eliminated");
STATISTIC(BasicBlocksEliminated,  "Number of basic blocks entirely eliminated");
STATISTIC(BasicBlocksMerged,  "Number of basic blocks merged into its predecessors");

namespace {
class DeadCodeEliminationWithRangeAnalysis : public llvm::FunctionPass {
public:
  static char ID;
  DeadCodeEliminationWithRangeAnalysis() : FunctionPass(ID) {}
  virtual ~DeadCodeEliminationWithRangeAnalysis() {}
  
  virtual void deleteDeadBasicBlock(TerminatorInst *I){
    BasicBlock* Block = I->getParent();
    
    if(Block->getSinglePredecessor() == nullptr){
	if(dyn_cast<BranchInst>(I)){
	BranchInst* BI = dyn_cast<BranchInst>(I);
	if(Block->getSingleSuccessor()==nullptr){
	  TerminatorInst* Branch1 = BI->getSuccessor(0)->getTerminator();
	  TerminatorInst* Branch2 = BI->getSuccessor(1)->getTerminator();
	  BI->eraseFromParent();
	  InstructionsEliminated++;
	  for (Instruction &I: *Block)
	    InstructionsEliminated++;
	  Block->eraseFromParent();
	  BasicBlocksEliminated++;
	  deleteDeadBasicBlock(Branch1);
	  deleteDeadBasicBlock(Branch2);
	  
	}
	else{
	  TerminatorInst* Branch1 = BI->getSuccessor(0)->getTerminator();
	  BI->eraseFromParent();
	  InstructionsEliminated++;
	  for (Instruction &I: *Block)
	    InstructionsEliminated++;
	  Block->eraseFromParent();
	  BasicBlocksEliminated++;
	  deleteDeadBasicBlock(Branch1);
	}
      }
    }
  }
  
  virtual void mergeBasicBlocks(BasicBlock* B1, BasicBlock* B2){
    if(B2->getUniqueSuccessor()){
      BasicBlocksMerged++;
      BasicBlock* next2 = B2->getUniqueSuccessor();
      BasicBlock* next = B1;
      B1->getTerminator()->eraseFromParent();
      IRBuilder<> Builder(B1);
      Builder.CreateBr(B2);
      bool Merged = MergeBlockIntoPredecessor(B2);
      assert(Merged);
      mergeBasicBlocks(next, next2);
    }
  }
  
  virtual void deletePath(Instruction *I){ 
    if(dyn_cast<BranchInst>(I)){
      BranchInst *Branch = dyn_cast<BranchInst>(I);
      BasicBlock *Parent = Branch->getParent();
      BasicBlock *trueBlock = Branch->getSuccessor(0);
      BasicBlock *falseBlock = Branch->getSuccessor(1);
      
      trueBlock->removePredecessor(Parent);
      BranchInst *newBI = BranchInst::Create(falseBlock);
      ReplaceInstWithInst(Branch, newBI);
      
      deleteDeadBasicBlock(trueBlock->getTerminator());
      
      mergeBasicBlocks(Parent, falseBlock);
    }
  }
  
  
  virtual void handleICmpInst(BasicBlock::iterator I, InterProceduralRA<Cousot> &ra){
    ICmpInst *ICM = dyn_cast<ICmpInst>(I);
    Range r1 = ra.getRange(ICM->getOperand(0));
    Range r2 = ra.getRange(ICM->getOperand(1));
    I->dump();
    r1.print(errs());
    r2.print(errs());
    errs() <<"\n";
    //Below we handle the branchs that always takes the same path.
    //For each case of comparison (<.<=,>,>=,==,!=), the if statement handle the conditions that will never be true
    //and the else statement handle the conditios that will always be true
    
    switch(ICM->getPredicate()){
      case CmpInst::ICMP_SGT:
      case CmpInst::ICMP_UGT:
	if(r1.getUpper().sle(r2.getLower())){ //<= 
	  deletePath(ICM->getNextNode());
	}
	else if(r2.getUpper().sle(r1.getLower())){
	  BranchInst* BI = dyn_cast<BranchInst>(ICM->getNextNode());
	  BI->swapSuccessors();  //we swap the successors, so we can use the same function to delete then-paths and else-paths
	  deletePath(ICM->getNextNode());
	}
	break;
      case CmpInst::ICMP_SGE:
      case CmpInst::ICMP_UGE:
	if(r1.getUpper().slt(r2.getLower())){ //<
	  deletePath(ICM->getNextNode());
	}
	else if(r2.getUpper().sle(r1.getLower())){
	  BranchInst* BI = dyn_cast<BranchInst>(ICM->getNextNode());
	  BI->swapSuccessors();
	  deletePath(ICM->getNextNode());
	}
	break;
      case CmpInst::ICMP_SLT:
      case CmpInst::ICMP_ULT:
	if(r1.getLower().sge(r2.getUpper())){ //>=
	  deletePath(ICM->getNextNode());
	}
	else if(r2.getUpper().sle(r1.getLower())){
	  BranchInst* BI = dyn_cast<BranchInst>(ICM->getNextNode());
	  BI->swapSuccessors();
	  deletePath(ICM->getNextNode());
	}
	break;
      case CmpInst::ICMP_SLE:
      case CmpInst::ICMP_ULE:
	if(r1.getLower().sgt(r2.getUpper())){ //>
	  deletePath(ICM->getNextNode());
	}
	else if(r2.getUpper().sle(r1.getLower())){
	  BranchInst* BI = dyn_cast<BranchInst>(ICM->getNextNode());
	  BI->swapSuccessors();
	  deletePath(ICM->getNextNode());
	}
	break;
      case CmpInst::ICMP_EQ:
	if(r1.getLower() == r1.getUpper() && r2.getLower() == r2.getUpper()){
	  if(r1.getLower() != r2.getLower()){
	    deletePath(ICM->getNextNode());
	  }
	  else if(r2.getUpper().sle(r1.getLower())){
	    BranchInst* BI = dyn_cast<BranchInst>(ICM->getNextNode());
	    BI->swapSuccessors();
	    deletePath(ICM->getNextNode());
	  }
	}
	break;
      case CmpInst::ICMP_NE:
	if(r1.getLower() == r1.getUpper() && r2.getLower() == r2.getUpper()){
	  if(r1.getLower() == r2.getLower()){
	    deletePath(ICM->getNextNode());
	  }
	  else if(r2.getUpper().sle(r1.getLower())){
	    BranchInst* BI = dyn_cast<BranchInst>(ICM->getNextNode());
	    BI->swapSuccessors();
	    deletePath(ICM->getNextNode());
	  }
	}
	break;
    }
  }
  
  virtual bool runOnFunction(Function &F) {
    InterProceduralRA<Cousot> &ra = getAnalysis<InterProceduralRA<Cousot>>();
    
    errs() << "\nCousot Intra Procedural analysis (Values -> Ranges) of "
           << F.getName() << ":\n";
    for (Function::iterator bb = F.begin(), bbEnd = F.end(); bb != bbEnd;
         ++bb) {
      for (BasicBlock::iterator I = bb->begin(), IEnd = bb->end(); I != IEnd;
           ++I) {
	
	if(dyn_cast<ICmpInst>(I)){
	  handleICmpInst(I, ra);
	}
      }
    }
    return true;
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<InterProceduralRA<Cousot>>();
  }
};
}

char DeadCodeEliminationWithRangeAnalysis::ID = 0;
static RegisterPass<DeadCodeEliminationWithRangeAnalysis> X("dcewra",
                                "Dead Code Elimination Using Range Analysis");
