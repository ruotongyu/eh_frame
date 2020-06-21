#ifndef EHFRAME_PARSER_H
#define EHFRAME_PARSER_H

#include "dwarf.h"
#include "libdwarf.h"
#include <stdint.h>
#include <map>

class FrameData{
    private:
	bool _cfa_offset_sp; // is cfa offset to stack pointer in current frame
	uint64_t _lowpc;
	uint64_t _size;

	std::map<uint64_t, int32_t> frame_pointers; // stores the changing point of frame pointer
	
    public:

	FrameData(uint64_t pc, bool cfa_offset_sp, uint64_t size){
	    _lowpc = pc;
	    _cfa_offset_sp = cfa_offset_sp;
	    _size = size;
	}

	uint64_t get_pc(){return _lowpc; }

	bool in_range(uint64_t addr) { 
	    if (addr >= _lowpc && addr < _lowpc + _size) 
		return true; 
	    return false;
	}

	bool operator== (const FrameData& r_hs) {
	    return _lowpc == r_hs.getPC();
	}

	bool operator< (const FrameData& r_hs) {
	    return _lowpc < r_hs.getPC();
	}

	bool operator> (const FrameData& r_hs) {
	    return _lowpc > r_hs.getPC();
	}

	std::map<uint64_t, int32_t>* get_frame_pointers(){
	    return &frame_pointers;
	}

	void insert_frame_pointer(uint64_t addr, int32_t height){
	    frame_pointers[addr] = height;
	}

	int32_t get_height(uint64_t addr);

};

class FrameParser{

    private:
	std::set<FrameData> frames;

	void iter_frame(Dwarf_Debug, const char*);

	bool parse_fde(Dwarf_Cie, Dwarf_Error*);

	bool check_cfa_def(Dwarf_Frame_Op*, Dwarf_Signed);
    
    public:
	FrameParser(const char* f_path);

};
#endif
