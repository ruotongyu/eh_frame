using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;
using namespace InstructionAPI;
using namespace Dyninst::ParseAPI;


void getEhFrameAddrs(std::set<uint64_t>& pc_sets, const char* input, map<uint64_t, uint64_t> &functions);

set<uint64_t> loadGTFunc(char* input_pb, blocks::module& module, set<uint64_t> &functions);

map<uint64_t, uint64_t> loadGTRef(char* input_pb, RefInf::RefList &reflist, std::vector<SymtabAPI::Region *> regs);

void loadFnAddrs(char* input, map<uint64_t, uint64_t> &ref2func);
