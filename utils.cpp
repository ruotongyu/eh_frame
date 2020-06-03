
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

using namespace std;
using namespace Dyninst;
using namespace SymtabAPI;


bool isInGaps(map<unsigned long, unsigned long> gap_regions, unsigned ref){
	for(std::map<unsigned long, unsigned long>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
		unsigned long c_addr = (unsigned long) ref;
		if (c_addr > ite->first && c_addr < ite->second) {
			return true;
		}
	}
	return false;
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
			cout << hex <<  "Invalid " << ins->address << " " << ins->size << endl;
			break;
		}
		cout << hex <<  ins->address << " " << ins->size << endl;
		if (is_cs_nop_ins(ins) != 1){
			res_ins.insert(pcaddr);
		}
	}
	for (auto a : res_ins){
		cout << "Nop Instruction " << hex << a << endl;
	}
}

