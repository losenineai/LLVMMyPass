#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CFG.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>
#include "llvm/IR/Module.h"
#include<vector>
#include<list>
#include<ctime>
#include<map>
#include<cstdlib>
#include<utility>
using namespace llvm;
namespace
{
	struct TreeNode
	{
		unsigned int val=0,limit=0,l=0,r=0; //����������Ԫ�ظ���    [x,y)   x<=val<y
		TreeNode *left=NULL,*right=NULL;
	};
	struct MyFlatten : public FunctionPass
	{
    	static char ID;
   		MyFlatten() : FunctionPass(ID) {}

      	std::list<TreeNode*>::iterator randomElement(std::list<TreeNode*> *x)
		{
			std::list<TreeNode*>::iterator iter=x->begin();
			int val=x->size();
		    for(int i=0;i<rand()%val;i++)
		      	iter++;
			return iter;
		}
		void expandNode(TreeNode *node)
		{
			TreeNode *newNode=(TreeNode*)malloc(sizeof(TreeNode));
			newNode->left=newNode->right=NULL;
			node->left=newNode;
			newNode=(TreeNode*)malloc(sizeof(TreeNode));
			newNode->left=newNode->right=NULL;
			node->right=newNode;
		}
		int genRandomTree(TreeNode *node,int node_limit)
		{
		    std::list<TreeNode*> q;
		    q.push_back(node);
		    int node_num=1;
		    while(q.size()!=0 && node_num<node_limit)
		    {
		      	std::list<TreeNode*>::iterator tmp=randomElement(&q);
		      	TreeNode *node=*tmp;
		      	int val=(node->left==NULL)+(node->right==NULL);
			    if(val==2)
				{
					expandNode(node);
					q.push_back(node->left);
					q.push_back(node->right);
					q.erase(tmp);
				}
				node_num++;
			}
			q.clear();
			return node_num;
		}
		void walkTree(TreeNode *node)
		{
			if(node->left!=NULL)	//Traverse all branches
				walkTree(node->left);
			if(node->right!=NULL)
				walkTree(node->right);
			node->limit=0;
			if(node->left==NULL && node->right==NULL) //Start to calculate node info
				node->limit=1;
			else
				node->limit=node->left->limit+node->right->limit;
		}
		bool allocNode(TreeNode *node,unsigned int l,unsigned int r)
		{
			if(r-l<node->limit)
				return false;
			node->l=l;
			node->r=r;
			if(node->left==NULL && node->right==NULL)
				return true;
			unsigned int var;
			if(r-l-node->limit==0)
				var=0;
			else
				var=rand()%(r-l-node->limit);
			unsigned int mid=l+node->left->limit+var;
			if(!allocNode(node->left,l,mid) || !allocNode(node->right,mid,r))
				return false;
			return true;
		}
		BasicBlock *createRandomBasicBlock(TreeNode *node,Function *f,Value *var,std::vector<BasicBlock*>::iterator &iter,std::map<BasicBlock*,TreeNode*> *bb_info)
		{
			if(node->left==NULL && node->right==NULL)
			{
				BasicBlock *bb=*iter;
				bb_info->insert(std::pair<BasicBlock*,TreeNode*>(bb,node));
				iter++;
				return bb;
			}
			BasicBlock *left_bb=createRandomBasicBlock(node->left,f,var,iter,bb_info);
			BasicBlock *right_bb=createRandomBasicBlock(node->right,f,var,iter,bb_info);
			BasicBlock *node_bb=BasicBlock::Create(f->getContext(),"knot",f);
			if(node->left->r!=node->right->l)
				errs()<<"Error!\n";
			LoadInst *load=new LoadInst(Type::getInt32Ty(f->getContext()),var,"",node_bb);
			ICmpInst *condition=new ICmpInst(*node_bb,ICmpInst::ICMP_ULT,load,ConstantInt::get(Type::getInt32Ty(f->getContext()),node->left->r));
			BranchInst::Create(left_bb,right_bb,(Value*)condition,node_bb);
			return node_bb;
		}
		bool spawnRandomIf(BasicBlock *from,std::vector<BasicBlock*> *son,Value *var,std::map<BasicBlock*,TreeNode*> *bb_info)
		{
			TreeNode tree;
			genRandomTree(&tree,son->size());
			walkTree(&tree);
			if(!allocNode(&tree,0,0x7fffffff))
				return false;
			std::vector<BasicBlock*>::iterator iter=son->begin();
			BasicBlock *head=createRandomBasicBlock(&tree,from->getParent(),var,iter,bb_info);
			BranchInst::Create(head,from);
			return true;
		}
		std::vector<BasicBlock*> *getBlocks(Function *function,std::vector<BasicBlock*> *lists)
      	{
      		lists->clear();
      		for(BasicBlock &basicBlock:*function)
      			lists->push_back(&basicBlock);
      		return lists;
      	}
      	void getAnalysisUsage(AnalysisUsage &AU) const override
		{
			errs()<<"Require LowerSwitchPass\n";
			AU.addRequiredID(LowerSwitchID);
			FunctionPass::getAnalysisUsage(AU);
		}
		unsigned int getUniqueNumber(std::vector<unsigned int> *rand_list)
		{
			unsigned int num=rand();
			while(true)
			{
				bool state=true;
				for(std::vector<unsigned int>::iterator n=rand_list->begin();n!=rand_list->end();n++)
					if(*n==num)
					{
						state=false;
						break;
					}
				if(state)
					break;
				num=rand();
			}
			return num;
		}
		static bool valueEscapes(Instruction *Inst)
		{
			const BasicBlock *BB=Inst->getParent();
			for(const User *U:Inst->users())
			{
				const Instruction *UI=cast<Instruction>(U);
				if (UI->getParent()!=BB || isa<PHINode>(UI))
					return true;
			}
			return false;
		}
      	void DoFlatten(Function *f,int seed,int enderNum)
      	{
			srand(seed);
			std::vector<BasicBlock*> origBB;
			getBlocks(f,&origBB);
			if(origBB.size()<=1)
				return ;
			unsigned int rand_val=seed;
			Function::iterator tmp=f->begin();
			BasicBlock *oldEntry=&*tmp;
			origBB.erase(origBB.begin());
			BranchInst *firstBr=NULL;
			if(isa<BranchInst>(oldEntry->getTerminator()))
				firstBr=cast<BranchInst>(oldEntry->getTerminator());
			BasicBlock *firstbb=oldEntry->getTerminator()->getSuccessor(0);
			if((firstBr!=NULL && firstBr->isConditional()) || oldEntry->getTerminator()->getNumSuccessors()>2)		//Split the first basic block
			{
				BasicBlock::iterator iter=oldEntry->end();
				iter--;
				if(oldEntry->size()>1)
					iter--;
				BasicBlock *splited=oldEntry->splitBasicBlock(iter,Twine("FirstBB"));
				firstbb=splited;
				origBB.insert(origBB.begin(),splited);
			}
			unsigned int retBlockNum=0;
			for(std::vector<BasicBlock *>::iterator b=origBB.begin();b!=origBB.end();b++)
			{
				BasicBlock *bb=*b;
				if(bb->getTerminator()->getNumSuccessors()==0)
					retBlockNum++;
			}
			unsigned int loopEndNum=(enderNum>=(origBB.size()-retBlockNum)?(origBB.size()-retBlockNum):enderNum);
			BasicBlock *newEntry=oldEntry;												//Prepare basic block
			BasicBlock *loopBegin=BasicBlock::Create(f->getContext(),"LoopBegin",f,newEntry);
			std::vector<BasicBlock*> loopEndBlocks;
			for(int i=0;i<loopEndNum;i++)
			{
				BasicBlock *tmp=BasicBlock::Create(f->getContext(),"LoopEnd",f,newEntry);
				loopEndBlocks.push_back(tmp);
				BranchInst::Create(loopBegin,tmp);
			}

			newEntry->moveBefore(loopBegin);
		    newEntry->getTerminator()->eraseFromParent();
		    BranchInst::Create(loopBegin,newEntry);

		    AllocaInst *switchVar=new AllocaInst(Type::getInt32Ty(f->getContext()),0,Twine("switchVar"),newEntry->getTerminator());		//Create switch variable
		    std::map<BasicBlock*,TreeNode*> bb_map;
		    std::map<BasicBlock*,unsigned int> nums_map;
		    spawnRandomIf(loopBegin,&origBB,switchVar,&bb_map);
		    unsigned int startNum=0;
			for(std::vector<BasicBlock *>::iterator b=origBB.begin();b!=origBB.end();b++)
			{
				BasicBlock *bb=*b;
				unsigned int l=bb_map[bb]->l,r=bb_map[bb]->r;
				unsigned int val=rand()%(r-l)+l;
				nums_map[bb]=val;
				if(bb==firstbb)
					startNum=val;
			}
			int every=(int)((double)(origBB.size()-retBlockNum)/(double)loopEndNum);
			errs()<<f->getName()<<" "<<every<<" "<<loopEndNum<<" "<<origBB.size()-retBlockNum<<"\n";

			int counter=0;
			std::vector<BasicBlock *>::iterator end_iter=loopEndBlocks.begin();
			for(std::vector<BasicBlock *>::iterator b=origBB.begin();b!=origBB.end();b++)							//Handle successors
			{
				BasicBlock *block=*b;
				BasicBlock *loopEnd=*end_iter;
				if(block->getTerminator()->getNumSuccessors()==1)
				{
					//errs()<<"This block has 1 successor\n";
					BasicBlock *succ=block->getTerminator()->getSuccessor(0);
					ConstantInt *caseNum=cast<ConstantInt>(ConstantInt::get(Type::getInt32Ty(f->getContext()),nums_map[succ]));
					block->getTerminator()->eraseFromParent();
					new StoreInst(caseNum,switchVar,block);
					BranchInst::Create(loopEnd,block);
					counter++;
				}
				else if(block->getTerminator()->getNumSuccessors()==2)
				{
					//errs()<<"This block has 2 successors\n";
					BasicBlock *succTrue=block->getTerminator()->getSuccessor(0);
					BasicBlock *succFalse=block->getTerminator()->getSuccessor(1);
					ConstantInt *numTrue=cast<ConstantInt>(ConstantInt::get(Type::getInt32Ty(f->getContext()),nums_map[succTrue]));
					ConstantInt *numFalse=cast<ConstantInt>(ConstantInt::get(Type::getInt32Ty(f->getContext()),nums_map[succFalse]));
					BranchInst *oldBr=cast<BranchInst>(block->getTerminator());
					SelectInst *select=SelectInst::Create(oldBr->getCondition(),numTrue,numFalse,Twine("choice"),block->getTerminator());
					block->getTerminator()->eraseFromParent();
					new StoreInst(select,switchVar,block);
					BranchInst::Create(loopEnd,block);
					counter++;
				}
				if(counter==every)
				{
					counter=0;
					end_iter++;
					if(end_iter==loopEndBlocks.end())
						end_iter--;
				}
			}
			ConstantInt *startVal=cast<ConstantInt>(ConstantInt::get(Type::getInt32Ty(f->getContext()),startNum));		//Set the entry value
			new StoreInst(startVal,switchVar,newEntry->getTerminator());
			std::vector<PHINode *> tmpPhi;
		    std::vector<Instruction *> tmpReg;
			BasicBlock *bbEntry = &*f->begin();
			do
			{
				tmpPhi.clear();
				tmpReg.clear();
				for(Function::iterator i = f->begin();i!=f->end();i++)
				{
					for( BasicBlock::iterator j=i->begin();j!=i->end();j++)
					{
						if(isa<PHINode>(j))
						{
							PHINode *phi=cast<PHINode>(j);
							tmpPhi.push_back(phi);
							continue;
						}
						if (!(isa<AllocaInst>(j) && j->getParent()==bbEntry) && (valueEscapes(&*j) || j->isUsedOutsideOfBlock(&*i)))
						{
							tmpReg.push_back(&*j);
							continue;
						}
					}
				}
				for(unsigned int i=0;i<tmpReg.size();i++)
					DemoteRegToStack(*tmpReg.at(i),f->begin()->getTerminator());
				for(unsigned int i=0;i<tmpPhi.size();i++)
					DemotePHIToStack(tmpPhi.at(i),f->begin()->getTerminator());
			}
			while(tmpReg.size()!= 0 || tmpPhi.size()!= 0);
			errs()<<"Finish\n";
      	}
   		bool runOnFunction(Function &function) override
		{
			DoFlatten(&function,time(0),10);
			//DoSplit(&function,4);
      		return false;
    	}
  	};
}

char MyFlatten::ID=0;
static RegisterPass<MyFlatten> X("myfla", "MyFlatten");

// Register for clang
static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
  [](const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
    PM.add(new MyFlatten());
  });
