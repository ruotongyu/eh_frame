#include "tailcall.h"
#include "CFG.h"
#include <vector>

#define DEBUG_TAIL_CALL

tailCallAnalyzer::tailCallAnalyzer(ParseAPI::CodeObject* _co, std::map<uint64_t, uint64_t>* _refs, std::map<uint64_t, uint64_t>* _funcs_range, const char* _f_path){
    codeobj = _co;
    refs = _refs;
    funcs_range = _funcs_range;
    cached_func = 0;
    cached_sa = 0;
    frame_parser = new FrameParser(_f_path);
}

tailCallAnalyzer::~tailCallAnalyzer(){
    delete frame_parser;

    if (cached_sa)
	delete cached_sa;
}

void tailCallAnalyzer::analyze(std::map<uint64_t, uint64_t>& merged_funcs){
    std::set<uint64_t> targets;
    std::map<uint64_t, ParseAPI::Function*> all_funcs;

    std::set<uint64_t> new_funcs;
    std::set<uint64_t> deleted_funcs;
    std::set<uint64_t> indirect_jump_targets;
    
    uint64_t cur_func_addr;
    int64_t cur_func_end;

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
		} else if(succ->type() == ParseAPI::INDIRECT){
		    // collect all indirect jump targets
		    indirect_jump_targets.insert(succ->trg()->start());
		}
	    }
	}
    }

    // iterate all jump edges
    for(auto func: codeobj->funcs()){

	cur_func_addr = func->addr();
	
	auto tmp_func_iter = funcs_range->find(cur_func_addr);

	if(tmp_func_iter != funcs_range->end()){
	    cur_func_end = tmp_func_iter->second;
	}else{
	    cur_func_end = -1;
	}

	for(auto bb: func->blocks()){
	    for(auto succ: bb->targets()){

		if (succ->trg()->start() == 0xffffffffffffffff)
		    continue;

		target = succ->trg()->start();

		// if target in the range of current function
		// skip
		if (cur_func_end != -1 && 
			target >= cur_func_addr && target < cur_func_end){
		    continue;
		}

		switch(succ->type()){
		    // bin: do not consider indirect jump for now.
		    case ParseAPI::COND_TAKEN:
		    case ParseAPI::DIRECT:

			if (getStackHeight(bb->lastInsnAddr(), func, bb, height)){

#ifdef DEBUG_TAIL_CALL
			    std::cerr << "[Tail call detection]: The height in " << std::hex << bb->lastInsnAddr() << " : " << height << std::endl;
#endif
			    // check if the height of stack is balanced
			    if ((height == 8 || height == 4)){

				// the target is already a function.
				// skip.
				if (all_funcs.find(target) != all_funcs.end()){
				    continue;
				}

				// this is the targets of indirect jump.
				// skip.
				if (indirect_jump_targets.find(target) != indirect_jump_targets.end()){
				    continue;
				}

				// there are other references to the target
				if(targets.find(target) != targets.end()){
#ifdef DEBUG_TAIL_CALL
					std::cerr << "[Tail call detection]: at " << std::hex << succ->src()->start() << 
					    ", the target " << succ->trg()->start() << " is a function!" << std::endl;
#endif
					new_funcs.insert(target);

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
				    " to function " << func->addr() << "!" << std::endl;
#endif
				merged_funcs[target] = func->addr();
				deleted_funcs.insert(target);
				}
			    }
			} // end if(getStackHeight...)
			else{
#ifdef DEBUG_TAIL_CALL
			    std::cerr << "[Tail call detection]: Can't get height of address " << std::hex << bb->lastInsnAddr() << std::endl;
#endif
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

    // TODO. find a better way to merge these functions
    /*
    for (auto func_addr: deleted_funcs){

	auto cur_func_iter = all_funcs.find(func_addr);
	if (cur_func_iter == all_funcs.end())
		continue;

#ifdef DEBUT_TAIL_CALL
	std::cerr << "[Tail call detection]: delete function at "
	    << std::hex << func_addr << endl;
#endif
	std::cout << "destroy " << std::hex << func_addr << std::endl;

	// TODO.  merge the function
	//codeobj->destroy(cur_func_iter->second);
	//delete cur_func_iter->second;
    }
    */

}

bool tailCallAnalyzer::getStackHeight(uint64_t address, ParseAPI::Function* func, ParseAPI::Block* block, int32_t& height){
    bool ret_result = false;
    std::stringstream ss;
    std::vector<std::pair<Absloc, StackAnalysis::Height>> heights;

    // request stack height from ehframe first
    if (!frame_parser->request_stack_height(address, height)){
	return true;
    }


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
