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

using namespace Dyninst;
using namespace SymtabAPI;


std::map<unsigned long, unsigned long> dumpCFG(Dyninst::ParseAPI::CodeObject &codeobj){
	std::set<Dyninst::Address> seen;
	//std::set<Dyninst::Blocks> block_set;
	int count = 0;
	std::map<unsigned long, unsigned long> block_list;
	for (auto func:codeobj.funcs()){
		if(seen.count(func->addr())){
			continue;
		}
		seen.insert(func->addr());
		//count = count + 1;
		
		for (auto block: func->blocks()){
			block_list[(unsigned long) block->start()] = (unsigned long) block->end();
			std::cout << block->start() << "  " << block->end() << std::endl;
			//std::cout << block->end() << std::endl;
			//exit(1);
		}
	}
	//std::map<unsigned long, unsigned long> block_res;
	//std::map<Dyninst::Address, Dyninst::Address>::iterator it=block_list.begin();
       	//unsigned long start = (unsigned long) it->first;
	//while (it != block_list.end()){
	//	int x = (unsigned long) it->second;
	//	++it;
	//	int y = (unsigned long) it->first;
	//	if (y - x > 0) {
	//		block_res[start] = x;
	//		start = y;
	//	}
		//std::cout << x << " " << y << std::endl;
	//}
	//for  (const auto &[key, value]: block_res){
	//	std::cout << key << " " << value << std::endl;
		
	//}
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
	char* input_string = "/data/testsuite/libs/gcc_O2/libc-2.27.so.strip";
	getEhFrameAddrs(pc_sets, input_string);

	auto symtab_cs = std::make_shared<ParseAPI::SymtabCodeSource>(input_string);
	auto code_obj_eh = std::make_shared<ParseAPI::CodeObject>(symtab_cs.get());
	
	CHECK(code_obj_eh) << "Error: Fail to create ParseAPI::CodeObject";
	code_obj_eh->parse();
	
	for(auto addr : pc_sets){
		code_obj_eh->parse(addr, true);
	}
	//exit(1);
	//std::cout << count << std::endl;
	std::map<unsigned long, unsigned long> block_regions;
	block_regions=dumpCFG(*code_obj_eh);
	std::map<unsigned long, unsigned long> gap_regions;
	
	std::vector<SymtabAPI::Region *> regs;
	symtab_cs->getSymtabObject()->getCodeRegions(regs);
	std::map<Offset, unsigned long> CodeRegion;
	std::map<unsigned long, unsigned long>::iterator it=block_regions.begin();
	unsigned long last_end;
	for (auto &reg: regs){
		CodeRegion[reg->getMemOffset()] = reg->getMemSize();
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
		//std::cout << reg->getMemOffset() << std::endl;
		//std::cout << addr_end << std::endl;
		last_end = addr_end;
	}
	//std::cout << "Last " << last_end << " " << it->second << std::endl;
	if (last_end > (unsigned long) it->second){
		gap_regions[(unsigned long) it->second] = last_end;
	}
	//for(std::map<unsigned long, unsigned long>::iterator ite=gap_regions.begin(); ite!=gap_regions.end();++ite) {
	//	std::cout << ite->first << "  " << ite->second <<std::endl;
	//}

}
