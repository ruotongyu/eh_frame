#include <stdio.h>
#include <sstream>
#include <fstream>
#include <set>
#include <cstdint>
#include "CodeObject.h"
#include "Function.h"
#include "Symtab.h"
#include "Instruction.h"
#include "glog/logging.h"
#include "gflags/gflags.h"
#include <sys/stat.h>
#include <iostream>
#include <capstone/capstone.h>
#include <Dereference.h>
#include "refInf.pb.h"
#include "blocks.pb.h"

using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;
using namespace InstructionAPI;
using namespace Dyninst::ParseAPI;

//#define DEBUG

void identifiedWrong(set<uint64_t> identified, set<uint64_t> gt_functions, uint64_t plt_start, uint64_t plt_end, set<uint64_t> nops) {
	set<uint64_t> result;
	//cout << "plt Section >> " << hex << plt_start << " " << plt_end << endl;
	for (auto func: identified){
		if (!gt_functions.count(func) && !nops.count(func)){
			if (func < plt_start || func > plt_end){
				//cout << "New False Positive: " << hex << func << endl;
				result.insert(func);
			}
		}
	}

	cout << "New False Positive: " << hex << result.size() << endl;
}

void PrintFuncResult(int raw_eh_num, int reu_eh_num, int gt_num) {
	cout << "Number of Ground Truth Functions: " << dec << gt_num << endl;
	cout << "Number of Missing Functions from EhFrame: " << dec << raw_eh_num << endl;
	cout << "Number of Missing Functions from Recursive Disassemble EHFrame: " << dec << reu_eh_num << endl;
}

void DebugDisassemble(Dyninst::ParseAPI::CodeObject &codeobj) {
	set<Address> seen;
	for (auto func:codeobj.funcs()){
		if(seen.count(func->addr())){
			continue;
		}
		seen.insert(func->addr());
		for(auto block: func->blocks()){
			Dyninst::ParseAPI::Block::Insns instructions;
			block->getInsns(instructions);
			uint64_t cur_addr = block->start();
			for (auto it: instructions){
				Dyninst::InstructionAPI::Instruction inst = it.second;
				if (!inst.isLegalInsn() || !inst.isValid()) {
					cout << "Invalid Instruction: " << hex << cur_addr << endl;
				}
				cout << "Inst: 0x" << hex << cur_addr << " " << inst.format() << endl; 
				cur_addr += inst.size();
			}
		}
	}
}

void getFunctions(set<uint64_t> identified, set<uint64_t> fn_functions, set<uint64_t> &undetect, set<uint64_t> &fixed){
	for (auto fuc: fn_functions){
		if (identified.count(fuc)){
			fixed.insert(fuc);
		} else{
			undetect.insert(fuc);
		}
	}
}

void PrintRefInGaps(set<uint64_t> fnInGap, map<uint64_t, uint64_t> gt_ref, map<uint64_t, uint64_t> &withRef){
	int NoRef = 0;
	for (auto fn : fnInGap){
		if (!gt_ref[fn]){
			NoRef++;
		}else{
			withRef[fn] = gt_ref[fn];
		}
	}
	int WithRef = fnInGap.size() - NoRef;
	cout << "FN in gaps with Ref: " << dec << WithRef << endl;
        cout <<	"FN in gaps without Ref: " << dec << NoRef << endl;
	//for (auto ref: withRef){
	//	cout << "Address: " << hex << ref.first << " Ref:" << ref.second << endl;
	//}
}

void functionInGaps(set<uint64_t> fn_functions, set<uint64_t> &fnInGap, set<uint64_t> &fnNotGap, map<uint64_t, uint64_t> gap_regions) {
	for (auto func: fn_functions){
		bool found = false;
		for (auto gap: gap_regions){
			if (func >= gap.first && func < gap.second){
				fnInGap.insert(func);
				found = true;
				break;
			}
		}
		if (!found) {
			fnNotGap.insert(func);
		}
	}
}


// get all cases where searched by basic block. Tail Call
set<uint64_t> printTailCall(set<uint64_t> fn_functions, set<uint64_t> pc_sets, set<uint64_t> bb_list) {
	set<uint64_t> tailCall;
	for (auto func : fn_functions){
		if (!pc_sets.count(func)){
			if (bb_list.count(func)){
				tailCall.insert(func);
			}
		}
	}
	return tailCall;
}


void printSet(set<uint64_t> p_set){
	for (auto ite: p_set){
		cout << hex << ite << endl;
	}
}

void printMap(map<uint64_t, uint64_t> p_map) {
	for (map<uint64_t, uint64_t>::iterator it=p_map.begin(); it!=p_map.end(); ++it){
		cout << hex << it->first << " " << it->second << endl;
	}
}

// Check if the address in gap regions
bool isInGaps(std::map<unsigned long, unsigned long> gap_regions, unsigned ref){
	for(std::map<unsigned long, unsigned long>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
		unsigned long c_addr = (unsigned long) ref;
		if (c_addr > ite->first && c_addr < ite->second) {
			return true;
		}
	}
	return false;
}

void getPltRegion(uint64_t &sec_start, uint64_t &sec_end, vector<SymtabAPI::Region *> regs){
	for (auto re : regs){
		if (re->getRegionName() == ".plt") {
			sec_start = (unsigned long) re->getMemOffset();
			sec_end = sec_start + re->getMemSize();
		}
	}
}

void Target2Addr(map<uint64_t, uint64_t> gt_ref, set<uint64_t> fn_functions){
	map<uint64_t, uint64_t> result;
	for(std::map<uint64_t, uint64_t>::iterator ite=gt_ref.begin(); ite!=gt_ref.end();++ite) {
		if (fn_functions.count(ite->second)) {
			result[ite->first] = ite->second;
			cout << "Found Target " << hex << ite->first << " " << ite->second << endl;
		}
	}
}

// compare the difference between two set, if flag is true return overlap function, else return difference
set<uint64_t> compareFunc(set<uint64_t> eh_functions, set<uint64_t> gt_functions, bool flag){
	set<uint64_t> res;
	for (auto func:gt_functions){
		if (flag) {
			if (eh_functions.count(func) && func!=0){
				res.insert(func);
			}
		} else{
			if (!eh_functions.count(func) && func!=0){
				res.insert(func);
			}
		}
	}
	return res;
}

void unionSet(set<Address> set1, set<Address> &set2){
	for (auto item : set1) {
		if (!set2.count(item)){
			set2.insert(item);
		}
	}
}


void ScanAddrInGap(map<uint64_t, uint64_t> gap_regions, set<Address> dataRef, set<Address> &RefinGap){
	// serach for result in gap regions
	for (auto ref : dataRef){
		for(std::map<uint64_t, uint64_t>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
			uint64_t c_addr = (uint64_t) ref;
			if (c_addr >= ite->first && c_addr < ite->second) {
				RefinGap.insert(c_addr);
#ifdef DEBUG
				cout << "ref in gap: " << hex << c_addr << " in gap: " << ite->first << " -> "
					<< ite->second << endl;
#endif
				break;
			}
		}
	}
}

class nopVisitor : public InstructionAPI::Visitor{
	public:
		nopVisitor() : foundReg(false), foundImm(false), foundBin(false), isNop(true) {}
		virtual ~nopVisitor() {}

		bool foundReg;
		bool foundImm;
		bool foundBin;
		bool isNop;

		virtual void visit(BinaryFunction*) {
			if (foundBin) isNop = false;
			if (!foundImm) isNop = false;
			if (!foundReg) isNop = false;
			foundBin = true;
		}

		virtual void visit(Immediate *imm) {
			if (imm != 0) isNop = false;
			foundImm = true;
		}

		virtual void visit(RegisterAST *) {
			foundReg = true;
		}

		virtual void visit(Dereference *){
			isNop = false;
		}
};

bool isNopInsn(Instruction insn) {
	if(insn.getOperation().getID() == e_nop){
		return true;
	}
	if(insn.getOperation().getID() == e_lea){
		set<Expression::Ptr> memReadAddr;
		insn.getMemoryReadOperands(memReadAddr);
		set<RegisterAST::Ptr> writtenRegs;
		insn.getWriteSet(writtenRegs);

		if(memReadAddr.size() == 1 && writtenRegs.size() == 1) {
			if (**(memReadAddr.begin()) == **(writtenRegs.begin())) {
				return true;
			}
		}
		nopVisitor visitor;

		insn.getOperand(1).getValue()->apply(&visitor);
		if (visitor.isNop) {
			return true;
		}
	}
	return false;
}


void ScanGaps(map<uint64_t, uint64_t> gap_regions, map<uint64_t, uint64_t> scanTarget){
	set<Address> gap_set;
	// serach for result in gap regions
	for (auto item : scanTarget){
		bool found = false;
		for(std::map<uint64_t, uint64_t>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
			if (item.first >= ite->first && item.first <= ite->second) {
				//gap_set.insert(a_addr);
				found = true;
				break;
			}
		}
		if (!found) {
			cout << "Ref: " << hex << item.first << " Target: " << item.second << endl;
		}
	}
}


map<uint64_t, uint64_t> getGaps(map<uint64_t, uint64_t> functions, vector<SymtabAPI::Region *> regs, uint64_t &gap_regions_num){
	std::map<uint64_t, uint64_t> gap_regions;
	std::map<uint64_t, uint64_t>::iterator it=functions.begin();
	unsigned long last_end;
	for (auto &reg: regs){
		uint64_t addr = (uint64_t) reg->getMemOffset();
		uint64_t addr_end = addr + (uint64_t) reg->getMemSize();
		uint64_t start = (uint64_t) it->first;
		if (addr_end <= start) {
			continue;
		}
		if (start > addr) {
			gap_regions[addr] = start;
			++gap_regions_num;
		}
		while (it != functions.end()){
       			uint64_t block_end = (uint64_t) it->second;
       			++it;
			uint64_t block_start = (uint64_t) it->first;
			if (block_end > addr_end){
				std::cout << "Error: Check Region" << std::endl;
				cout << hex << block_end << " " << addr_end << endl;
				exit(1);
			}
			if (block_start < addr_end){
				if (block_start > block_end){
					gap_regions[block_end] = block_start;
					++gap_regions_num;
				}
			}else{
				if (addr_end > block_end){
					gap_regions[block_end] = addr_end;
					++gap_regions_num;
				}
				break;
			}
		}
		last_end = addr_end;
		if (it == functions.end()) {
			break;
		}
	}
	if (it != functions.end() && last_end > it->second){
		gap_regions[it->second] = last_end;
		++gap_regions_num;
	}
	return gap_regions;
}

