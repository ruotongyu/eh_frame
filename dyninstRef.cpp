/* Reference: https://github.com/trailofbits/mcsema/blob/master/tools/mcsema_disass/dyninst/
 * Date: 10/10/2019
 * Author: binpang
 *
 * Extract the cfg from dyninst
 *
 * Build: make
 */

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


using namespace Dyninst;
DEFINE_string(binary, "", "Path to striped binary file");
DEFINE_string(output, "/tmp/dyninstRef.pb", "Path to output file");
DEFINE_int32(speculative, 1, "The mode of speculative parsing. 0 represents do not parse. 1 represents using idiom, 2 represents using preamble, 3 represents using both. default is 1");
std::set<uint64_t> matchingFunc;
int total_funcs = 0;

void outputMatchingFunc(const char* output_sta){
  auto input_string = FLAGS_binary.data();
  auto input_file = const_cast<char* >(input_string);
  std::fstream output(output_sta, std::ios::out | std::ios::trunc);
  output << "===================Function Matching Information:==========================\n";
  DLOG(INFO) << "===================Function Matching Information:==========================\n";
  int count = 0;
  for (auto func: matchingFunc){
    DLOG(INFO) << "Matching Func#" << count << ": " << std::hex << func << std::endl;
    output << "Func #" << count << ": " << std::hex << func << std::endl;
    count++;
  }
  output << "All function numbers: " << total_funcs << std::endl;
  output << "Function matching numbers: " << count << std::endl;
  output << "Function matching rate: " << (float)count / total_funcs << std::endl;
  output.close();
}

int main(int argc, char** argv){

  std::stringstream ss;
  ss << " " << argv[0] << "\\" << std::endl
    << "      --binary INPUT_FILE \\" << std::endl
    << "      --output OUTPUT PB FILE \\" << std::endl
    << "      --speculative SPECULATIVE MODE \\" << std::endl
    << "      --statics STATICS DATA" << std::endl;

  FLAGS_logtostderr = 1;
  // Parse the command line arguments
  google::InitGoogleLogging(argv[0]);
  google::SetUsageMessage(ss.str());
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(!FLAGS_binary.empty()) << "Input file need to be specified!";
  LOG(INFO) << "Config: binary path " << FLAGS_binary << "\n"
    << "output file is " << FLAGS_output << "\n"
    << "speculative mode is " << FLAGS_speculative << "\n" << std::endl;
  
  auto input_string = FLAGS_binary.data();
  auto input_file = const_cast<char* >(input_string);
  auto symtab_cs = std::make_shared<ParseAPI::SymtabCodeSource>(input_file);
  CHECK(symtab_cs) << "Error during creation of ParseAPI::SymtabCodeSource!";
  SymtabAPI::Symtab *symTab;
  std::string binaryPathStr(input_file);
  bool isParsable = SymtabAPI::Symtab::openFile(symTab, binaryPathStr);
  if(isParsable == false){
      const char *error = "error: file can not be parsed";
      std::cout << error;
      return - 1;
  }


  auto code_obj = std::make_shared<ParseAPI::CodeObject>(symtab_cs.get());
  CHECK(code_obj) << "Error during creation of ParseAPI::CodeObject";

  code_obj->parse();

  // module pb
  // check version. we use two versions. 9.3.2 and 
  // Use some speculative parsing to do function matching
  auto preamble = Dyninst::ParseAPI::GapParsingType::PreambleMatching;
  auto idiom = Dyninst::ParseAPI::GapParsingType::IdiomMatching;
  if (FLAGS_speculative){
    for (auto &reg: symtab_cs->regions()) {
      switch(FLAGS_speculative){
	case 1:
	  code_obj->parseGaps(reg, idiom);
	  break;
	case 2:
	  code_obj->parseGaps(reg, preamble);
	  break;
	case 3:
	  code_obj->parseGaps(reg, idiom);
	  code_obj->parseGaps(reg, preamble);
	  break;
	default:
	  break;
      }
    }
  }
  std::vector<SymtabAPI::Region *> allRegions;
  symTab->getAllRegions(allRegions);
  std::vector<SymtabAPI::Region *>::iterator reg_it;
  for(reg_it = allRegions.begin(); reg_it != allRegions.end(); ++reg_it) {
     std::vector<SymtabAPI::relocationEntry> &region_rels = (*reg_it)->getRelocations();
     std::vector<SymtabAPI::relocationEntry>::iterator rel_it;
     for(rel_it = region_rels.begin(); rel_it != region_rels.end(); ++rel_it) {
       std::cout << "relocation: " << std::hex << rel_it->rel_addr() << " -> " << std::hex << rel_it->target_addr() << std::endl;
   }
  }

  //dumpCFG(pbModule, *code_obj);
  return 0;
}
