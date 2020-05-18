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
		if (is_cs_nop_ins(ins) == 1){
			res_ins.insert(pcaddr);
		}
	//	cout << is_cs_nop_ins(ins) << endl;

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
	symtab_cs->getSymtabObject()->getCodeRegions(regs);
	
	std::map<unsigned long, unsigned long> gap_regions;
	//gap_regions = gapRegions(regs, block_regions);
	
	std::map<unsigned long, unsigned long>::iterator it=block_regions.begin();
	unsigned long last_end;
	for (auto &reg: regs){
		//std::cout << reg->getRegionName() << std::endl;
		unsigned long addr = (unsigned long) reg->getMemOffset();
		unsigned long addr_end = addr + (unsigned long) reg->getMemSize();
		unsigned long start = (unsigned long) it->first;
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
	}
	if (last_end > (unsigned long) it->second){
		gap_regions[(unsigned long) it->second] = last_end;
	}

	set<uint64_t> res;
	
	for(std::map<unsigned long, unsigned long>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
		
		is_inst_nop(ite->first, ite->second, file_offset, input_string, res);
	//	exit(1);
	}
	for (uint64_t addr : res){
		std::cout << hex << addr <<std::endl;
	}

}
