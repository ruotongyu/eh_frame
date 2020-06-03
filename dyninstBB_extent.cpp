#include <stdio.h>
#include <sstream>
#include <fstream>
#include <set>
#include <cstdint>
#include "CodeObject.h"
#include "Function.h"
#include "Symtab.h"
#include "Instruction.h"
#include "BinaryFunction.h"
#include "glog/logging.h"
#include "gflags/gflags.h"
#include "blocks.pb.h"
#include <sys/stat.h>
#include <iostream>
#include <capstone/capstone.h>
#include <Dereference.h>
#include <InstructionAST.h>
#include <Result.h>
#include "refInf.pb.h"
#include "blocks.pb.h"
#include "utils.h"
#include "loadInfo.h"

using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;
using namespace InstructionAPI;
using namespace Dyninst::ParseAPI;

bool Inst_help(Dyninst::ParseAPI::CodeObject &codeobj, set<Address> &res, set<unsigned> all_instructions, map<unsigned long, unsigned long> gap_regions, set<unsigned> &dis_inst, set<uint64_t> &nops){
	set<Address> seen;
	for (auto func: codeobj.funcs()){
		if(seen.count((unsigned long) func->addr())){
			continue;
		}

		seen.insert((unsigned long) func->addr());
		res.insert(func->addr());
		for (auto block: func->blocks()){
			//cout << hex << block->start() << " " << block->end() << endl;
			Dyninst::ParseAPI::Block::Insns instructions;
			block->getInsns(instructions);
			unsigned cur_addr = block->start();
			//Check control flow graph
			for (auto succ: block->targets()){
				unsigned succ_addr = succ->trg()->start();
				if (succ_addr == 4294967295){
					continue;
				}
				if (!all_instructions.count(succ_addr)){
					if (!isInGaps(gap_regions, succ_addr)){
						return false;
					}
				}
			}
			// go through all instructions
			for (auto it: instructions) {
				dis_inst.insert(cur_addr);
				Dyninst::InstructionAPI::Instruction inst = it.second;
				//Check inlegall instruction
				if (!inst.isLegalInsn() || !inst.isValid()){
					//cout << std::hex << it.first << endl;
					return false;
				}
				//if (cur_addr >= 4653134 and cur_addr < 4653174){
				//	cout << hex << cur_addr << " " << inst.format() << endl;
				//}
				//Check conflict instructions
				if (!all_instructions.count(cur_addr)) {
					if (!isInGaps(gap_regions, cur_addr)){
						return false;
					}
				}
				if (isNopInsn(inst)) {
					nops.insert(cur_addr);
				}
				cur_addr += inst.size();
			}
		}
	}
	return true;
}

set<uint64_t> CheckInst(set<Address> addr_set, char* input_string, set<unsigned> instructions, map<unsigned long, unsigned long> gap_regions, map<uint64_t, Address> &Add2Ref, set<uint64_t> &dis_addr, map<uint64_t, set<unsigned>> &inst_list, set<uint64_t> &nops) {
	set<uint64_t> identified_functions;
	for (auto addr: addr_set){
		auto symtab_cs = std::make_shared<ParseAPI::SymtabCodeSource>(input_string);
		auto code_obj_gap = std::make_shared<ParseAPI::CodeObject>(symtab_cs.get());
		CHECK(code_obj_gap) << "Error: Fail to create ParseAPI::CodeObject";
		code_obj_gap->parse(addr, true);
		set<Address> func_res;
		set<unsigned> dis_inst;
		if (Inst_help(*code_obj_gap, func_res, instructions, gap_regions, dis_inst, nops)){
			//cout << "Disassembly Address is 0x" << hex << addr << endl;
			dis_addr.insert((uint64_t) addr);
			inst_list[(uint64_t) addr] = dis_inst;
			for (auto r_f : func_res){
				Add2Ref[(uint64_t) r_f] = addr;
				identified_functions.insert((uint64_t) r_f);
				//cout << "Func  0x" <<std::hex << r_f << endl;
			}
			//cout << addr << endl;
		}
	}
	return identified_functions;
}

void expandFunction(Dyninst::ParseAPI::CodeObject &codeobj, map<uint64_t, uint64_t> &pc_funcs, set<uint64_t> &pc_sets) {
	std::set<Dyninst::Address> seen;
	for (auto func:codeobj.funcs()){
		if (seen.count(func->addr())){
			continue;
		}
		seen.insert(func->addr());
		bool found = false;
		for (map<uint64_t, uint64_t>::iterator it=pc_funcs.begin(); it != pc_funcs.end(); ++it){
			if (func->addr() >= it->first && func->addr() <= it->second){
				found = true;
				break;
			}
		}
		if (!found) {
			pc_sets.insert((uint64_t) func->addr());
			for (auto block: func->blocks()){
				pc_funcs[(uint64_t) block->start()] = (uint64_t) block->end();
			}
		}
	}
}

std::map<unsigned long, unsigned long> dumpCFG(Dyninst::ParseAPI::CodeObject &codeobj, set<unsigned> &all_instructions, set<uint64_t> &functions){
	std::set<Dyninst::Address> seen;
	std::map<unsigned long, unsigned long> block_list;
	for (auto func:codeobj.funcs()){
		if(seen.count(func->addr())){
			continue;
		}
		seen.insert(func->addr());
		functions.insert((uint64_t) func->addr());
		for (auto block: func->blocks()){
			block_list[(unsigned long) block->start()] = (unsigned long) block->end();
			Dyninst::ParseAPI::Block::Insns instructions;
			block->getInsns(instructions);
			unsigned cur_addr = block->start();
			for (auto p : instructions){
				Dyninst::InstructionAPI::Instruction inst = p.second;
				all_instructions.insert(cur_addr);
				cur_addr += inst.size();
				//cout << "Instruction Addr " << hex << cur_addr  << endl;
			}
		}
	}
	return block_list;
}



set<Address> getOperand(Dyninst::ParseAPI::CodeObject &codeobj, map<Address, Address> &ref_addr) {
	set<Address> constant;
	// pc pointer
	unsigned cur_addr = 0x0;
	unsigned next_addr = 0x0;
	for (auto func:codeobj.funcs()) {
		for (auto block: func->blocks()){
			Dyninst::ParseAPI::Block::Insns instructions;
			block->getInsns(instructions);

			// get current address
			for (auto it: instructions) {
				Dyninst::InstructionAPI::Instruction inst = it.second;
				InstructionAPI::RegisterAST thePC = InstructionAPI::RegisterAST::makePC(inst.getArch());

				cur_addr = it.first;
				next_addr = cur_addr + inst.size();
				if (cur_addr % 4 != 0) {
					continue;
				}
				std::vector<InstructionAPI::Operand> operands;
				inst.getOperands(operands);
				for (auto operand:operands){
					auto expr = operand.getValue();
					if (auto imm = dynamic_cast<InstructionAPI::Immediate *>(expr.get())) {
						Address addr = imm->eval().convert<Address>();
						constant.insert(addr);
						ref_addr[addr] = cur_addr;
						//cout << "constant Operand " << addr << endl;
					}
					else if (auto dref = dynamic_cast<InstructionAPI::Dereference *>(expr.get())){
						std::vector<InstructionAPI::InstructionAST::Ptr> args;
						dref->getChildren(args);

						if (auto d_expr = dynamic_cast<InstructionAPI::Expression *>(args[0].get())){
							std::vector<InstructionAPI::Expression::Ptr> exps;
							d_expr->getChildren(exps);
							Address ref_value;
							for (auto dref_expr : exps){
								if (auto dref_imm = dynamic_cast<InstructionAPI::Immediate *>(dref_expr.get())){
										ref_value = dref_imm->eval().convert<Address>();
										constant.insert(ref_value);
										ref_addr[ref_value] = cur_addr;
										//cout << "instruction addr: " << std::hex << cur_addr << " mem operand is " << std::hex << ref_value << endl;
										}
							}

							// bind the pc value.
							d_expr->bind(&thePC, InstructionAPI::Result(InstructionAPI::u64, next_addr));
							ref_value = d_expr->eval().convert<Address>();
							constant.insert(ref_value);
							ref_addr[ref_value] = cur_addr;
							//cout << "instruction addr: " << std::hex << cur_addr << " mem operand is " << std::hex << ref_value << endl;
						}
					}
				}
			}
		}
	}
	return constant;
}


set<Address> getDataRef(std::vector<SymtabAPI::Region *> regs, uint64_t offset, char* input, char* x64, map<Address, unsigned long> &RefMap){
	size_t code_size;
	struct stat results;
	string list[6] = {".rodata", ".data", ".fini_array", ".init_array", ".data.rel.ro", ".data.rel.ro.local"};
	set<string> white_list;
	for (int i = 0; i < 6; ++i) {
		white_list.insert(list[i]);
	}

	if (stat(input, &results) == 0) {
		code_size = results.st_size;
	}
	set<Address> DataRef_res;
	std::ifstream handleFile (input, std::ios::in | ios::binary);
	char buffer[code_size];
	handleFile.read(buffer, code_size);
	for (auto &reg: regs){
		if (!white_list.count(reg->getRegionName())){
			continue;	
		}
		
		unsigned long addr_start = (unsigned long) reg->getFileOffset();
		unsigned long m_offset = (unsigned long) reg->getMemOffset();
		//unsigned long addr_start = start - (unsigned long) offset; 
		unsigned long region_size = (unsigned long) reg->getMemSize();
		//void * d_buffer = (void *) &buffer[addr_start];
		for (int i = 0; i < region_size; ++i) {
			if (i + addr_start > code_size) {
				break;
			}
			if ((i + m_offset)%4 != 0) {
				continue;
			}
			Address addr;
			if (x64 == "x32"){
				unsigned int* res = (unsigned int*)(buffer + addr_start + i);
				addr = (Address) *res;
			}else {
				Address* res = (Address*)(buffer + addr_start + i);
				addr = *res;
			}
			//cout << hex << *res << "  "<<endl;
			DataRef_res.insert(addr);
			RefMap[addr] = i + m_offset;
		}
	}
	return DataRef_res;
}


int main(int argc, char** argv){
	std::set<uint64_t> pc_sets;
	map<uint64_t, uint64_t> pc_funcs;
	char* input_string = argv[1];
	char* input_pb = argv[2];
	char* input_block = argv[3];
	char* x64 = argv[4];
	// load false negative functions with reference
	map<uint64_t, uint64_t> ref2Addr;
	loadFnAddrs(input_string, ref2Addr);
	getEhFrameAddrs(pc_sets, input_string, pc_funcs);
	
	auto symtab_cs = std::make_shared<ParseAPI::SymtabCodeSource>(input_string);
	auto code_obj_eh = std::make_shared<ParseAPI::CodeObject>(symtab_cs.get());
	//printMap(pc_funcs);
	//exit(1);
	
	//get call instructions and functions from ground truth
	set<uint64_t> call_inst;
	set<uint64_t> gt_functions;
	blocks::module mModule;
	call_inst = loadGTFunc(input_block, mModule, gt_functions);
	CHECK(code_obj_eh) << "Error: Fail to create ParseAPI::CodeObject";
	code_obj_eh->parse();
	
	uint64_t file_offset = symtab_cs->loadAddress();
	//is_inst_nop(addr_s, addr_e, file_offset, input_string, tmp);
	
	
	for(auto addr : pc_sets){
		code_obj_eh->parse(addr, true);
	}
	expandFunction(*code_obj_eh, pc_funcs, pc_sets);
	
	// printMap(pc_funcs);
	//exit(1);
	//get instructions and functions disassemled from eh_frame
	set<unsigned> instructions;
	set<uint64_t> eh_functions;
	std::map<unsigned long, unsigned long> block_regions;
	block_regions=dumpCFG(*code_obj_eh, instructions, eh_functions);
	//get false negative functions
	set<uint64_t> fn_functions = compareFunc(eh_functions, gt_functions, false);
	
	std::vector<SymtabAPI::Region *> regs;
	std::vector<SymtabAPI::Region *> data_regs;
	symtab_cs->getSymtabObject()->getCodeRegions(regs);
	symtab_cs->getSymtabObject()->getDataRegions(data_regs);
	
	//get plt section region
	unsigned long plt_start, plt_end;
	getPltRegion(plt_start, plt_end, regs);
	
	// read reference ground truth from pb file
	map<uint64_t, uint64_t> gt_ref;
	RefInf::RefList refs_list;
	gt_ref = loadGTRef(input_pb, refs_list, data_regs);
	
	//get Tareget to Reference address
	//Target2Addr(gt_ref, fn_functions);
	//exit(1);
	//initialize gap regions
	map<Address, Address> ref_addr;
	set<Address> codeRef;
	codeRef = getOperand(*code_obj_eh, ref_addr);
	
	map<uint64_t, uint64_t> gap_regions;
	uint64_t gap_regions_num = 0;
	gap_regions = getGaps(pc_funcs, regs, gap_regions_num);
	
	//for (map<uint64_t, uint64_t>::iterator it=ref2Addr.begin(); it != ref2Addr.end(); ++it){
	//	for (map<uint64_t, uint64_t>::iterator ite=gap_regions.begin(); ite!=ref2Addr.end(); ++ite){
	//		if (it->first >= ite->first && it->first <= ite->second) {
	//			break;
	//		}
	//	}
	//	cout << "Not in gaps " << std::hex << it->first << " " << it->second << endl;
	//}
	//exit(1);
	//initialize data reference
	set<Address> dataRef;
	map<Address, unsigned long> DataRefMap;
	dataRef = getDataRef(data_regs, file_offset, input_string, x64, DataRefMap);
	
	//merge code ref and data ref
	set<Address> all_ref;
	unionSet(codeRef, dataRef);
	
	// search reference in gaps
	set<Address> GapRef;
	GapRef = ScanAddrInRegion(gap_regions, dataRef);
	//ScanGapsGT(gap_regions, gt_ref);
	
	// indentified functions is all the function start which generated from recursively disassemble 	   the functions found in gaps
	set<uint64_t> identified_functions;
	map<uint64_t, Address> Add2Ref;
	set<uint64_t> dis_addr;
	map<uint64_t, set<unsigned>> inst_list;
	set<uint64_t> nops;
	identified_functions = CheckInst(GapRef, input_string, instructions, gap_regions, Add2Ref, dis_addr, inst_list, nops);	
	set<uint64_t> tp_functions;
	tp_functions = compareFunc(fn_functions, identified_functions, true);
	
	// search fucntions not in ground truth.(new false positive)
	set<uint64_t> new_fp_functions;
	
	new_fp_functions = compareFunc(gt_functions, identified_functions, false);
	//Statical Result
	std::cout << "The number of gaps: " << std::dec << gap_regions_num << endl;
	cout << "The number of functions in gaps: " << fn_functions.size() << endl; 
	cout << "The number of functions from disassemble function in gaps: " << identified_functions.size() << endl;
	cout << "The number of correct functions: " << tp_functions.size() << endl;
	cout << "New False Positive number: " << new_fp_functions.size() << endl;
	int plt_num = 0;
	for (auto fuc: new_fp_functions) {
		if (nops.count(fuc)){
			cout << "Nop instruction: " << hex << fuc << endl;
			continue;
		}
		if (fuc < plt_start || fuc > plt_end){
			cout << hex << fuc << " " << Add2Ref[fuc] << " " << DataRefMap[Add2Ref[fuc]] << endl;
			set<unsigned> res = inst_list[fuc];
			for (auto it : res){
				cout << "Instructions: " << it << endl;
			}
		}else{
			++plt_num;
		}
	}
	cout << "fp in plt: " << dec << plt_num << endl;
	for (auto nop: nops) {
		cout << "Nops instruction: " << hex << nop << endl;
	}
}
