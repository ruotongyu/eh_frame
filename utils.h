using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;
using namespace InstructionAPI;
using namespace Dyninst::ParseAPI;


void printSet(set<uint64_t> p_set);

void printMap(map<uint64_t, uint64_t> p_map);

bool isInGaps(map<unsigned long, unsigned long> gap_regions, unsigned ref);

void Target2Addr(map<uint64_t, uint64_t> gt_ref, set<uint64_t> fn_functions);

void getPltRegion(unsigned long &sec_start, unsigned long &sec_end, vector<SymtabAPI::Region *> regs);

set<uint64_t> compareFunc(set<uint64_t> eh_functions, set<uint64_t> gt_functions, bool flag);

set<Address> unionSet(set<Address> set1, set<Address> set2);

set<Address> ScanAddrInRegion(map<unsigned long, unsigned long> gap_regions, set<Address> dataRef);

class nopVisitor : public InstructionAPI::Visitor{
	public:
		nopVisitor() : foundReg(false), foundImm(false), foundBin(false), isNop(true){}
		virtual ~nopVisitor();

		bool foundReg;
		bool foundImm;
		bool foundBin;
		bool isNop;

		virtual void visit(BinaryFunction*);

		virtual void visit(Immediate *imm);
			
		virtual void visit(RegisterAST *);

		virtual void visit(Dereference *);
};

bool isNopInsn(Instruction insn);


void ScanGapsGT(map<unsigned long, unsigned long> gap_regions, map<uint64_t, uint64_t> dataRef);

map<uint64_t, uint64_t> getGaps(map<uint64_t, uint64_t> functions, vector<SymtabAPI::Region *> regs, uint64_t &gap_regions_num);
