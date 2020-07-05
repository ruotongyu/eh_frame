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
#include "Reference.h"

using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;
using namespace InstructionAPI;
using namespace Dyninst::ParseAPI;


void CCReference(Dyninst::ParseAPI::CodeObject &codeobj, vector<SymtabAPI::Region *>& code_regs, map<uint64_t, uint64_t> &RefMap, set<unsigned> &instructions){
	getCodeReference(codeobj, RefMap);
	cout << "Map Size: " << RefMap.size() << endl;
	uint64_t textSec, textSecSize;
	std::string text (".text");
	for (auto reg : code_regs){
		if (text.compare(reg->getRegionName()) == 0){
			textSec = (uint64_t) reg->getMemOffset();
			textSecSize = (uint64_t) reg->getMemSize();
		}
	}
	for (auto ref: RefMap){
		if (ref.first < textSec || ref.first >= textSec + textSecSize) {
			RefMap.erase(ref.first);
			continue;
		}
		if (ref.second >= textSec && ref.second < textSec + textSecSize){
			if (instructions.count(ref.second) == 0) {
				RefMap.erase(ref.first);
				cout << "Invalid Reference: " << hex << ref.first << ",  Target:" << ref.second << endl;
			} else{
				cout << "Code Reference: " << hex << ref.first << ",  Target:" << ref.second << endl;
			}
		} else{
			RefMap.erase(ref.first);
		}
	}
	cout << "Size of Map: " << dec << RefMap.size() << endl;
}

void DCReference(Dyninst::ParseAPI::CodeObject &codeobj, vector<SymtabAPI::Region *>& data_regs, vector<SymtabAPI::Region *>& code_regs, map<uint64_t, uint64_t> &RefMap, uint64_t offset, char* input, char* x64, set<unsigned> &instructions) {
	getDataReference(data_regs, offset, input, x64, RefMap);
	uint64_t textSec, textSecSize;
	std::string text (".text");
	for (auto reg : code_regs){
		if (text.compare(reg->getRegionName()) == 0){
			textSec = (uint64_t) reg->getMemOffset();
			textSecSize = (uint64_t) reg->getMemSize();
		}
	}
	for (auto ref: RefMap){
		if (ref.second >= textSec && ref.second < textSec + textSecSize){
			if (instructions.count(ref.second) == 0) {
				RefMap.erase(ref.first);
				cout << "Invalid Reference: " << hex << ref.first << ",  Target:" << ref.second << endl;
			} else{
				cout << "Code Reference: " << hex << ref.first << ",  Target:" << ref.second << endl;
			}
		}
	}else{
		RefMap.erase(ref.first);
	}
}

	
void getDataReference(std::vector<SymtabAPI::Region *>& regs, uint64_t offset, char* input, char* x64, map<uint64_t, uint64_t> &RefMap){
	size_t code_size;
	struct stat results;
	char *buffer;
	string list[6] = {".rodata", ".data", ".fini_array", ".init_array", ".data.rel.ro", ".data.rel.ro.local"};
	set<string> white_list;
	for (int i = 0; i < 6; ++i) {
		white_list.insert(list[i]);
	}

	if (stat(input, &results) == 0) {
		code_size = results.st_size;
	}
	std::ifstream handleFile (input, std::ios::in | ios::binary);

	buffer = new char[code_size + 1];
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
			RefMap[(uint64_t) (i + m_offset)] = (uint64_t) (addr);
		}
	}
	delete buffer;
}


void getCodeReference(Dyninst::ParseAPI::CodeObject &codeobj, map<uint64_t, uint64_t> &RefMap) {
	Address ref_value;
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
					if (auto imm = dynamic_cast<InstructionAPI::Immediate *>(expr.get())) { // immediate operand
						Address addr = imm->eval().convert<Address>();
						RefMap[(uint64_t) cur_addr] = (uint64_t) addr;
#ifdef DEBUG
						cout << "[ref imm]: instruction at " << hex << cur_addr << " ref target is " << addr << endl;
#endif
					}
					else if (auto dref = dynamic_cast<InstructionAPI::Dereference *>(expr.get())){ // memeory operand
						std::vector<InstructionAPI::InstructionAST::Ptr> args;
						dref->getChildren(args);

						if (auto d_expr = dynamic_cast<InstructionAPI::Expression *>(args[0].get())){
							std::vector<InstructionAPI::Expression::Ptr> exps;

							// bind the pc value.
							d_expr->bind(&thePC, InstructionAPI::Result(InstructionAPI::u64, next_addr));
							ref_value = d_expr->eval().convert<Address>();
							if (ref_value){
								RefMap[(uint64_t) cur_addr] = (uint64_t) ref_value;
#ifdef DEBUG
								cout << "[ref mem]: instruction at " << hex << cur_addr << " ref target is " << ref_value << endl;
#endif
								continue; // do not iterate over exprs
							}

							d_expr->getChildren(exps);
							for (auto dref_expr : exps){
								if (auto dref_imm = dynamic_cast<InstructionAPI::Immediate *>(dref_expr.get())){
										ref_value = dref_imm->eval().convert<Address>();
										RefMap[(uint64_t) cur_addr] = (uint64_t) ref_value;
										//cout << "instruction addr: " << std::hex << cur_addr << " mem operand is " << std::hex << ref_value << endl;
										}
							}

						}
					} else if (auto binary_func = dynamic_cast<InstructionAPI::BinaryFunction *>(expr.get())){ // binary operand. such as add,sub const(%rip)
						if (isCFInst(&inst))
							continue;

						binary_func->bind(&thePC, InstructionAPI::Result(InstructionAPI::u64, next_addr));
						ref_value = binary_func->eval().convert<Address>();
						if (ref_value){
							RefMap[(uint64_t) cur_addr] = (uint64_t) ref_value;
#ifdef DEBUG
							cout << "[ref bin]: instruction at " << hex << cur_addr << " ref target is " << ref_value << endl;
#endif
						}
					}
				}
			}
		}
	}
}


bool isCFInst(InstructionAPI::Instruction* ins){
	switch (ins->getCategory()){
		case InstructionAPI::c_CallInsn:
		case InstructionAPI::c_BranchInsn:
		case c_ReturnInsn:
			return true;
			break;
		default:
			return false;
	}
}


