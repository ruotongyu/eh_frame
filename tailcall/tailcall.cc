#include "tailcall.h"
#include "CFG.h"
#include <vector>

#define DEBUG_TAIL_CALL

tailCallAnalyzer::tailCallAnalyzer(ParseAPI::CodeObject* _co, std::map<uint64_t, uint64_t>* _refs, const char* _f_path){
    codeobj = _co;
    refs = _refs;
    cached_func = 0;
    cached_sa = 0;
    frame_parser = new FrameParser(_f_path);
}

tailCallAnalyzer::~tailCallAnalyzer(){
    delete frame_parser;

    if (cached_sa)
	delete cached_sa;
}

void tailCallAnalyzer::analyze(){
    std::set<uint64_t> targets;
    std::map<uint64_t, ParseAPI::Function*> all_funcs;

    std::set<uint64_t> new_funcs;
    std::set<uint64_t> deleted_funcs;

    int32_t height;
    uint64_t target;

    for (auto ref: *refs){
	targets.insert(ref.second);
    }

    // find the target of call instructions
    for(auto func: codeobj->funcs()){

	all_funcs[func->addr()] = func;

	for (auto bb: func->blocks()){
	    for(auto succ: bb->targets()){
		if (succ->type() == ParseAPI::CALL){
		    targets.insert(succ->trg()->start());
		}
	    }
	}
    }

    // iterate all jump edges
    for(auto func: codeobj->funcs()){
	for(auto bb: func->blocks()){
	    for(auto succ: bb->targets()){
		switch(succ->type()){
		    case ParseAPI::COND_TAKEN:
		    case ParseAPI::DIRECT:
		    case ParseAPI::INDIRECT:
			if (getStackHeight(bb->lastInsnAddr(), func, bb, height)){
			    target = succ->trg()->start();

			    // check if the height of stack is balanced
			    if ((height == -8 || height == -4)){

				// there are other references to the target
				if(targets.find(target) != targets.end()){
#ifdef DEBUG_TAIL_CALL
				std::cerr << "[Tail call detection]: at " << std::hex << succ->src()->start() << 
				    ", the target " << succ->trg()->start() << " is a function!" << std::endl;
#endif
				if (all_funcs.find(target) == all_funcs.end()){
				    new_funcs.insert(target);
				    }
				}
			    } 

			    else{
				// detect non-continues 'function' caused by the entry in ehframe
				// firstly, the stack height is not equal to 0
				// secondly, there is no referecnes to the target except for current jump
				if(all_funcs.find(target) != all_funcs.end() 
					&& targets.find(target) == targets.end()){
#ifdef DEBUG_TAIL_CALL
				std::cerr << "[Tail call detection]: merge function at " << std::hex << succ->trg()->start() << 
				    " to function " << succ->src()->start() << "!" << std::endl;
#endif
				deleted_funcs.insert(target);
				}
			    }
			}
			break;
		}
	    }
	}
    }

    for (auto func_addr: new_funcs){
#ifdef DEBUG_TAIL_CALL
	std::cerr << "[Tail call detection]: create a new function at " 
	    << std::hex << func_addr << std::endl;
#endif
	codeobj->parse(func_addr, false);
    }

    for (auto func_addr: deleted_funcs){

	auto cur_func_iter = all_funcs.find(func_addr);
#ifdef DEBUT_TAIL_CALL
	std::cerr << "[Tail call detection]: delete function at "
	    << std::hex << func_addr << endl;
#endif
	codeobj->destroy(cur_func_iter->second);
	// TODO. merge the functions
    }

}

bool tailCallAnalyzer::getStackHeight(uint64_t address, ParseAPI::Function* func, ParseAPI::Block* block, int32_t& height){
    bool ret_result = false;
    std::stringstream ss;
    std::vector<std::pair<Absloc, StackAnalysis::Height>> heights;

    // request stack height from ehframe first
    if (!frame_parser->request_stack_height(address, height))
	return true;

    // othersize, get stackheight from stack analysis of dyninst
    if (cached_func != func->addr()){

	if(cached_sa)
	    delete cached_sa;

	cached_sa = new StackAnalysis(func);
	cached_func = func->addr();
    }

    cached_sa->findDefinedHeights(block, address, heights);
    for (auto iter = heights.begin(); iter != heights.end(); iter++){
	const Absloc &loc = iter->first;
	if (!loc.isSP()){
	    continue;
	}

	StackAnalysis::Height &s_height = iter->second;
	if (s_height.isTop() || s_height.isBottom()){
	    continue;
	}
	ss << s_height;
	ss >> height;
	height = height * -1;
	ret_result = true;
	ss.clear();
	break;
    }

    return ret_result;
}
