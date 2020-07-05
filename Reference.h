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
#include "Location.h"
#include "livenessAnaEhframe.h"
#include "bitArray.h"

using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;
using namespace InstructionAPI;
using namespace Dyninst::ParseAPI;

void ScanAllReference(Dyninst::ParseAPI::CodeObject &codeobj, vector<SymtabAPI::Region *>& regs, map<uint64_t, uint64_t> &RefMap, uint64_t offset, char* input, char* x64);

void getDataReference(std::vector<SymtabAPI::Region *>& regs, uint64_t offset, char* input, char* x64, map<uint64_t, uint64_t> &RefMap);

void getCodeReference(Dyninst::ParseAPI::CodeObject &codeobj, map<uint64_t, uint64_t> &RefMap);

bool isCFInst(InstructionAPI::Instruction* ins);
