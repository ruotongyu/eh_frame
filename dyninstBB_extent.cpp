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
#include "blocks.pb.h"
#include <sys/stat.h>
#include <iostream>
#include <capstone/capstone.h>
#include <Dereference.h>
#include <InstructionAST.h>
#include <Result.h>
#include "refInf.pb.h"
#include "blocks.pb.h"


using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;


// Check if the address in gap regions
bool isInGaps(map<unsigned long, unsigned long> gap_regions, unsigned ref){
	for(std::map<unsigned long, unsigned long>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
		unsigned long c_addr = (unsigned long) ref;
		if (c_addr > ite->first && c_addr < ite->second) {
			return true;
		}
	}
	return false;
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

set<uint64_t> compareFunc(set<uint64_t> eh_functions, set<uint64_t> gt_functions){
	set<uint64_t> res;
	for (auto func:gt_functions){
		if (!eh_functions.count(func) && func!=0){
			//cout << "0x" << hex << func << endl;
			res.insert(func);
		}
	}
	return res;
}
set<uint64_t> loadInfo(char* input_pb, blocks::module& module, set<uint64_t> &functions) {
	set<uint64_t> call_inst;
	std::fstream input(input_pb, std::ios::in | std::ios::binary);
	if (!input) {
		cout << "Could not open the file " << input_pb << endl;
	}
	if (!module.ParseFromIstream(&input)){
		cout << "Could not load pb file" << input_pb << endl;
	}
	for (auto func : module.fuc()) {
		functions.insert(func.va());
		for (auto bb : func.bb()){
			for (auto inst : bb.instructions()){
				if (inst.call_type() == 3){
					call_inst.insert(inst.va());			
				}
			}
		}
	}
	return call_inst;
}

int is_cs_nop_ins(cs_insn *ins){
	switch(ins->id){
		case X86_INS_NOP:
		case X86_INS_FNOP:
		case X86_INS_INT3:
			return 1;
		default:
			return 0;
	}
}

void is_inst_nop(unsigned long addr_start, unsigned long addr_end, uint64_t offset, char* input, set<uint64_t> &res_ins){
	struct stat results;
	size_t code_size;
	if (stat(input, &results) == 0) {
		code_size = results.st_size;
	}
	std::ifstream handleFile (input, std::ios::in | ios::binary);
	char buffer[code_size];
	handleFile.read(buffer, code_size);

	csh dis;
	cs_insn *ins;
	cs_open(CS_ARCH_X86, CS_MODE_64, &dis);
	uint64_t pcaddr = (uint64_t) addr_start;
	uint64_t off = (uint64_t)addr_start - offset;
	uint8_t const *code = (uint8_t *) &buffer[off];
	size_t size = size_t(addr_end - addr_start);
	//std::cout << hex << size << "  " << offset << std::endl;
	ins = cs_malloc(dis);
	while(cs_disasm_iter(dis, &code, &size, &pcaddr, ins)){
		if (!ins->address || !ins->size) {
			break;
		}
		if (is_cs_nop_ins(ins) != 1){
			res_ins.insert(pcaddr);
		}
	//	cout << is_cs_nop_ins(ins) << endl;
	}
}

bool Inst_help(Dyninst::ParseAPI::CodeObject &codeobj, set<Address> &res, set<unsigned> all_instructions, map<unsigned long, unsigned long> gap_regions){
	set<Address> seen;
	for (auto func: codeobj.funcs()){
		if(seen.count((unsigned long) func->addr())){
			continue;
		}
		seen.insert((unsigned long) func->addr());
		res.insert(func->addr());
		for (auto block: func->blocks()){
			Dyninst::ParseAPI::Block::Insns instructions;
			block->getInsns(instructions);
			unsigned cur_addr = block->start();
			// get current address
			for (auto it: instructions) {
				Dyninst::InstructionAPI::Instruction inst = it.second;
				if (!inst.isLegalInsn()){
					//cout << std::hex << it.first << endl;
					return false;
				}
				if (!all_instructions.count(cur_addr)) {
					if (!isInGaps(gap_regions, cur_addr)){
						return false;
					}
				}
				cur_addr += inst.size();
			}
		}
	}
	return true;
}

void CheckInst(set<Address> addr_set, char* input_string, set<unsigned> instructions, map<unsigned long, unsigned long> gap_regions) {
	
	for (auto addr: addr_set){
		auto symtab_cs = std::make_shared<ParseAPI::SymtabCodeSource>(input_string);
		auto code_obj_gap = std::make_shared<ParseAPI::CodeObject>(symtab_cs.get());
		CHECK(code_obj_gap) << "Error: Fail to create ParseAPI::CodeObject";
		code_obj_gap->parse();
		code_obj_gap->parse(addr, true);
		set<Address> func_res;
		if (Inst_help(*code_obj_gap, func_res, instructions, gap_regions)){
			cout << "Disassembly Address is 0x" << hex << addr << endl;
			for (auto r_f : func_res){
				cout << "Func  0x" <<std::hex << r_f << endl;
			}
			//cout << addr << endl;
		}
	}
}

std::map<unsigned long, unsigned long> dumpCFG(Dyninst::ParseAPI::CodeObject &codeobj, set<unsigned> &all_instructions, set<uint64_t> &functions){
	std::set<Dyninst::Address> seen;
	int count = 0;
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


void getEhFrameAddrs(std::set<uint64_t>& pc_sets, const char* input){
	std::stringstream ss;
	ss << "readelf --debug-dump=frames " << input << " | grep pc | cut -f3 -d = | cut -f1 -d . > /tmp/Dyninst_tmp_out.log";
	system(ss.str().c_str());
	std::ifstream frame_file("/tmp/Dyninst_tmp_out.log");
	std::string line;
	if (frame_file.is_open()){
		while(std::getline(frame_file, line)){
			uint64_t pc_addr = std::stoul(line, nullptr, 16);
			pc_sets.insert(pc_addr);
		}
	}
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



set<Address> getDataRef(std::vector<SymtabAPI::Region *> regs, uint64_t offset, char* input, char* x64){
	size_t code_size;
	struct stat results;
	if (stat(input, &results) == 0) {
		code_size = results.st_size;
	}
	set<Address> DataRef_res;
	std::ifstream handleFile (input, std::ios::in | ios::binary);
	char buffer[code_size];
	handleFile.read(buffer, code_size);
	for (auto &reg: regs){
		unsigned long addr_start = (unsigned long) reg->getMemOffset();
		addr_start = addr_start - (unsigned long) offset; 
		unsigned long region_size = (unsigned long) reg->getMemSize();
		//void * d_buffer = (void *) &buffer[addr_start];
		for (int i = 0; i < region_size; ++i) {
			if (i + addr_start > code_size) {
				break;
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
		}
	}
	return DataRef_res;
}

set<Address> ScanGaps(map<unsigned long, unsigned long> gap_regions, set<Address> dataRef){
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

map<uint64_t, uint64_t> loadGroundTruth(char* input_pb, RefInf::RefList &reflist, std::vector<SymtabAPI::Region *> regs) {
	map<uint64_t, uint64_t> target_addr;
	std::fstream input(input_pb, std::ios::in | std::ios::binary);
	if (!input) {
		cout << "Could not open the file " << input_pb << endl;
	}
	if (!reflist.ParseFromIstream(&input)){
		cout << "Could not load pb file" << input_pb << endl;
	}
	uint64_t target_va, ref_va;
	for (int i = 0; i < reflist.ref_size(); i++){
		const RefInf::Reference& cur_ref = reflist.ref(i);
		ref_va = cur_ref.ref_va();
		target_va = cur_ref.target_va();
		//target_addr[ref_va] = target_va;
		//cout << hex << target_va << endl;
		for (auto &reg: regs) {
			unsigned long addr_start = (unsigned long) reg->getMemOffset();
			unsigned long addr_end = addr_start + (unsigned long) reg->getMemSize();
			if (ref_va >= addr_start && ref_va <= addr_end){
				target_addr[ref_va] = target_va;
				//cout << hex << "0x" << ref_va << " 0x" << target_va << endl;
				break;
			}
		}
	}
	return target_addr;
}



int main(int argc, char** argv){
	std::set<uint64_t> pc_sets;
	char* input_string = argv[1];
	char* input_pb = argv[2];
	char* input_block = argv[3];
	char* x64 = argv[4];
	getEhFrameAddrs(pc_sets, input_string);
	
	auto symtab_cs = std::make_shared<ParseAPI::SymtabCodeSource>(input_string);
	auto code_obj_eh = std::make_shared<ParseAPI::CodeObject>(symtab_cs.get());
	
	//get call instructions and functions from ground truth
	set<uint64_t> call_inst;
	set<uint64_t> gt_functions;
	blocks::module mModule;
	call_inst = loadInfo(input_block, mModule, gt_functions);
	CHECK(code_obj_eh) << "Error: Fail to create ParseAPI::CodeObject";
	code_obj_eh->parse();
	
	//is_inst_nop();
	uint64_t file_offset = symtab_cs->loadAddress();
	//cout << hex << symtab_cs->loadAddress() << endl;
	for(auto addr : pc_sets){
		code_obj_eh->parse(addr, true);
	}
	
	
	//get instructions and functions disassemled from eh_frame
	set<unsigned> instructions;
	set<uint64_t> eh_functions;
	std::map<unsigned long, unsigned long> block_regions;
	block_regions=dumpCFG(*code_obj_eh, instructions, eh_functions);
	//get false negative functions
	set<uint64_t> fn_functions = compareFunc(eh_functions, gt_functions);
	
	std::vector<SymtabAPI::Region *> regs;
	std::vector<SymtabAPI::Region *> data_regs;
	symtab_cs->getSymtabObject()->getCodeRegions(regs);
	symtab_cs->getSymtabObject()->getDataRegions(data_regs);
	
	// read reference ground truth from pb file
	map<uint64_t, uint64_t> gt_ref;
	RefInf::RefList refs_list;
	gt_ref = loadGroundTruth(input_pb, refs_list, data_regs);
	
	//get Tareget to Reference address
	//Target2Addr(gt_ref, fn_functions);
	//exit(1);
	//initialize gap regions
	std::map<unsigned long, unsigned long> gap_regions;
	uint64_t gap_regions_num = 0;
	map<Address, Address> ref_addr;
	set<Address> cons_addr;
	cons_addr = getOperand(*code_obj_eh, ref_addr);
	std::map<unsigned long, unsigned long>::iterator it=block_regions.begin();
	unsigned long last_end;

	for (auto &reg: regs){
		//std::cout << reg->getRegionName() << std::endl;
		unsigned long addr = (unsigned long) reg->getMemOffset();
		unsigned long addr_end = addr + (unsigned long) reg->getMemSize();
		unsigned long start = (unsigned long) it->first;
		//cout << "0x" << std::hex << addr << " " << addr_end << endl;
		if (start > addr) {
			gap_regions[addr] = start;
			++gap_regions_num;
		}
		while (it != block_regions.end()){
       			unsigned long block_end = (unsigned long) it->second;
       			++it;
			unsigned long block_start = (unsigned long) it->first;
			if (block_end > addr_end){
				std::cout << "Error: Check Region" << std::endl;
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
		if (it == block_regions.end()) {
			break;
		}
	}
	if (it != block_regions.end() && last_end > (unsigned long) it->second){
		gap_regions[(unsigned long) it->second] = last_end;
		++gap_regions_num;
	}

	//initialize data reference
	set<Address> dataRef;
	dataRef = getDataRef(data_regs, file_offset, input_string, x64);
	set<Address> GapRef;
	GapRef = ScanGaps(gap_regions, dataRef);
	//ScanGapsGT(gap_regions, gt_ref);
	CheckInst(GapRef, input_string, instructions, gap_regions);	

	//Statical Result
	std::cout << "The number of gaps is " << std::dec << gap_regions_num << endl;
	
}
