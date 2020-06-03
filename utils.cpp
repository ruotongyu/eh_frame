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

// Check if the address in gap regions
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

bool isInGaps(std::map<unsigned long, unsigned long> gap_regions, unsigned ref){
	for(std::map<unsigned long, unsigned long>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
		unsigned long c_addr = (unsigned long) ref;
		if (c_addr > ite->first && c_addr < ite->second) {
			return true;
		}
	}
	return false;
}

void getPltRegion(unsigned long &sec_start, unsigned long &sec_end, vector<SymtabAPI::Region *> regs){
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

set<Address> unionSet(set<Address> set1, set<Address> set2){
	for (auto item : set1) {
		if (!set2.count(item)){
			set2.insert(item);
		}
	}
	return set2;
}


set<Address> ScanAddrInRegion(map<unsigned long, unsigned long> gap_regions, set<Address> dataRef){
	set<Address> gap_set;
	// serach for result in gap regions
	for (auto ref : dataRef){
		for(std::map<unsigned long, unsigned long>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
			unsigned long c_addr = (unsigned long) ref;
			if (c_addr > ite->first && c_addr < ite->second) {
				gap_set.insert(c_addr);
				//cout << "0x" << std::hex << c_addr << endl;
				break;
			}
		}
	}
	return gap_set;
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


void ScanGapsGT(map<unsigned long, unsigned long> gap_regions, map<uint64_t, uint64_t> dataRef){
	set<Address> gap_set;
	map<uint64_t, uint64_t>::iterator mt;
	// serach for result in gap regions
	for (mt = dataRef.begin(); mt != dataRef.end(); ++mt){
		for(std::map<unsigned long, unsigned long>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
			unsigned long c_addr = (unsigned long) mt->second;
			if (c_addr > ite->first && c_addr < ite->second) {
				//gap_set.insert(a_addr);
				cout << "0x" << std::hex << mt->first << " " << mt->second << endl;
				break;
			}
		}
	}
}


map<uint64_t, uint64_t> getGaps(map<uint64_t, uint64_t> functions, vector<SymtabAPI::Region *> regs, uint64_t &gap_regions_num){
	std::map<uint64_t, uint64_t> gap_regions;
	std::map<uint64_t, uint64_t>::iterator it=functions.begin();
	unsigned long last_end;
	for (auto &reg: regs){
		unsigned long addr = (unsigned long) reg->getMemOffset();
		unsigned long addr_end = addr + (unsigned long) reg->getMemSize();
		unsigned long start = (unsigned long) it->first;
		if (addr_end <= start) {
			continue;
		}
		if (start > addr) {
			gap_regions[addr] = start;
			++gap_regions_num;
		}
		while (it != functions.end()){
       			unsigned long block_end = (unsigned long) it->second;
       			++it;
			unsigned long block_start = (unsigned long) it->first;
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
	if (it != functions.end() && last_end > (unsigned long) it->second){
		gap_regions[(unsigned long) it->second] = last_end;
		++gap_regions_num;
	}
	return gap_regions;
}

