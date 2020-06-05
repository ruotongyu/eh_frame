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
/*
void CheckLinker(set<uint64_t> &fn_functions, const char* input){
	std::stringstream ss;
	ss << "objdump -d " << input << " > /tmp/Dyninst_tmp_dump";
	system(ss.str().c_str());
}*/
void getEhFrameAddrs(std::set<uint64_t>& pc_sets, const char* input, map<uint64_t, uint64_t> &functions){
	std::stringstream ss;
	ss << "readelf --debug-dump=frames " << input << " | grep pc | cut -f3 -d =  > /tmp/Dyninst_tmp_out.log";
	system(ss.str().c_str());
	std::ifstream frame_file("/tmp/Dyninst_tmp_out.log");
	std::string line;
	std::string delimiter = "..";
	if (frame_file.is_open()){
		while(std::getline(frame_file, line)){
			string start = line.substr(0, line.find(delimiter));
			string end = line.substr(line.find(delimiter));
			end = end.substr(2);
			uint64_t pc_addr = std::stoul(start, nullptr, 16);
			uint64_t func_end = std::stoul(end, nullptr, 16);
			pc_sets.insert(pc_addr);
			functions[pc_addr] = func_end;
		}
	}
}

set<uint64_t> loadGTFunc(char* input_pb, blocks::module& module, set<uint64_t> &functions) {
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


// load ground truth reference from protocol buffer
// save as target to reference
map<uint64_t, uint64_t> loadGTRef(char* input_pb, RefInf::RefList &reflist) {
	map<uint64_t, uint64_t> target_addr;
	std::fstream input(input_pb, std::ios::in | std::ios::binary);
	if (!input) {
		cout << "Could not open the file " << input_pb << endl;
		exit(1);
	}
	if (!reflist.ParseFromIstream(&input)){
		cout << "Could not load pb file" << input_pb << endl;
		exit(1);
	}
	uint64_t target_va, ref_va;
	for (int i = 0; i < reflist.ref_size(); i++){
		const RefInf::Reference& cur_ref = reflist.ref(i);
		ref_va = cur_ref.ref_va();
		target_va = cur_ref.target_va();
		target_addr[target_va] = ref_va;
		//cout << hex << target_va << endl;
	}
	return target_addr;
}

void loadFnAddrs(char* input, map<uint64_t, uint64_t> &ref2func){
	std::ifstream file_name("./script/fn_with_reference.log");
	std::string line;
	string name = input;
	if (file_name.is_open()){
		string target_file;
		while(std::getline(file_name, line)) {
			if (line.find("data") == 1){
				target_file = line;
			}
			if (name.compare(target_file) == 0 && line.find("Target") == 0) {
				int start_index = line.find("0x") + 2;
				int end_index = line.find(",") - 1;
				int length = end_index - start_index;
				string ref = line.substr(end_index);
				int ref_start = ref.find("0x") + 2;
				uint64_t func_addr = std::stoi(line.substr(start_index, length), 0, 16);
				uint64_t ref_addr = std::stoi(ref.substr(ref_start, length), 0, 16);
				ref2func[ref_addr] = func_addr;
				//cout << hex << func_addr << " " << ref_addr << endl;
			}
		}
	}
}

