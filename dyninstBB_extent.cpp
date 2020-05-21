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



using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;


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

bool Inst_help(Dyninst::ParseAPI::CodeObject &codeobj, set<Address> &res){
	set<Address> seen;
	for (auto func: codeobj.funcs()){
		if(seen.count((unsigned long) func->addr())){
			continue;
		}
		seen.insert((unsigned long) func->addr());
		for (auto block: func->blocks()){
			Dyninst::ParseAPI::Block::Insns instructions;
			block->getInsns(instructions);
			// get current address
			for (auto it: instructions) {
				Dyninst::InstructionAPI::Instruction inst = it.second;
				res.insert(it.first);
				if (!inst.isLegalInsn()){
					//cout << std::hex << it.first << endl;
					return false;
				}
			}
		}
	}
	return true;
}

void CheckInst(set<Address> addr_set, char* input_string) {
	for (auto addr: addr_set){
		auto symtab_cs = std::make_shared<ParseAPI::SymtabCodeSource>(input_string);
		auto code_obj_gap = std::make_shared<ParseAPI::CodeObject>(symtab_cs.get());
		CHECK(code_obj_gap) << "Error: Fail to create ParseAPI::CodeObject";
		code_obj_gap->parse();
		code_obj_gap->parse(addr, true);
		set<Address> func_res;
		if (Inst_help(*code_obj_gap, func_res)){
			cout << addr << endl;
			for (auto r_f : func_res){
				cout << std::hex << r_f << endl;
			}
			//cout << addr << endl;
		}
	}
}

std::map<unsigned long, unsigned long> dumpCFG(Dyninst::ParseAPI::CodeObject &codeobj){
	std::set<Dyninst::Address> seen;
	int count = 0;
	std::map<unsigned long, unsigned long> block_list;
	for (auto func:codeobj.funcs()){
		if(seen.count(func->addr())){
			continue;
		}
		seen.insert(func->addr());
		
		for (auto block: func->blocks()){
			block_list[(unsigned long) block->start()] = (unsigned long) block->end();
			//std::cout << block->start() << "  " << block->end() << std::endl;
			//std::cout << block->end() << std::endl;
			//exit(1);
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

void getDataRef(std::vector<SymtabAPI::Region *> regs, uint64_t offset, char* input){
	size_t code_size;
	struct stat results;
	if (stat(input, &results) == 0) {
		code_size = results.st_size;
	}
	std::ifstream handleFile (input, std::ios::in | ios::binary);
	char buffer[code_size];
	handleFile.read(buffer, code_size);
	for (auto &reg: regs){
		unsigned long addr_start = (unsigned long) reg->getMemOffset();
		addr_start = addr_start - (unsigned long) offset; 
		unsigned long region_size = (unsigned long) reg->getMemSize();
		//void * d_buffer = (void *) &buffer[addr_start];
		//cout << hex << d_buffer;
		//cout << hex << ++d_buffer;
		for (int i = 0; i < region_size; ++i) {
			Address* res = (Address*)(buffer + addr_start + i);
			cout << hex << *res << "  ";
		}
		//cout << offset << endl;
		exit(1);
	}

}


int main(int argc, char** argv){
	std::set<uint64_t> pc_sets;
	char* input_string = argv[1];
	getEhFrameAddrs(pc_sets, input_string);
	
	auto symtab_cs = std::make_shared<ParseAPI::SymtabCodeSource>(input_string);
	auto code_obj_eh = std::make_shared<ParseAPI::CodeObject>(symtab_cs.get());
	
	CHECK(code_obj_eh) << "Error: Fail to create ParseAPI::CodeObject";
	code_obj_eh->parse();
	
	//is_inst_nop();
	uint64_t file_offset = symtab_cs->loadAddress();
	//cout << hex << symtab_cs->loadAddress() << endl;
	for(auto addr : pc_sets){
		code_obj_eh->parse(addr, true);
	}
	
	std::map<unsigned long, unsigned long> block_regions;
	block_regions=dumpCFG(*code_obj_eh);
	
	std::vector<SymtabAPI::Region *> regs;
	std::vector<SymtabAPI::Region *> data_regs;
	symtab_cs->getSymtabObject()->getCodeRegions(regs);
	symtab_cs->getSymtabObject()->getDataRegions(data_regs);
	getDataRef(data_regs, file_offset, input_string);
	std::map<unsigned long, unsigned long> gap_regions;
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
				}
			}else{
				if (addr_end > block_end){
					gap_regions[block_end] = addr_end;
				}
				break;
			}
		}
		last_end = addr_end;
		if (it == block_regions.end()) {
			break;
		}
		//cout << "0x" << std::hex << ia->first << " " << ia->second << endl;
		//cout << "0x" << std::hex << addr << " " << addr_end << endl;
	}
	//for (auto res : block_regions) {
	//	cout << "0x" << std::hex << res.first << " " << res.second << endl;
	//}
	//exit(1);
	if (it != block_regions.end() && last_end > (unsigned long) it->second){
		gap_regions[(unsigned long) it->second] = last_end;
	}

	//set<uint64_t> res;
	set<unsigned long> func_list;
	set<Address> gap_set;
	for (auto addr: cons_addr){
		for(std::map<unsigned long, unsigned long>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
			unsigned long c_addr = (unsigned long) addr;
			if (c_addr > ite->first && c_addr < ite->second) {
				gap_set.insert(addr);
				//cout << "0x" << std::hex << c_addr << endl;
				if (ite->second - ite->first > 10) {
					cout << "0x" << std::hex << ite->first << " " << ite->second << endl;
				}
				break;
			}
		}
	}
	//CheckInst(gap_set, input_string);	
	//for (auto addr : gap_set){
	//	std::cout << hex << (unsigned long) ref_addr[addr] << "  " << (unsigned long) addr << std::endl;
	//}
}
