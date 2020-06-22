#include <fcntl.h>
#include <unistd.h>
#include <iostream>

#include "EhframeParser.h"

#define DWARF_DEBUG

#define UNDEF_VAL 2000
#define SAME_VAL 2001
#define CFA_VAL 2002
#define INITIAL_VAL UNDEF_VAL

using namespace std;

FrameParser::FrameParser(const char* f_path){
    int fd = -1;
    int res = DW_DLV_ERROR;
    int regtabrulecount = 0;
    Dwarf_Error error;
    Dwarf_Handler errhand = 0;
    Dwarf_Ptr errarg = 0;
    Dwarf_Debug dbg = 0;

    fd = open(f_path, O_RDONLY);

    if (fd < 0){
        std::cerr << "Can't open the file " << f_path << endl; 
	exit(-1);
    }

    res = dwarf_init_b(fd, DW_DLC_READ, DW_GROUPNUMBER_ANY,
	    errhand, errarg, &dbg, &error);

    if (res != DW_DLV_OK){
	cerr << "Parse dwarf error!" << endl;
	
	if (res == DW_DLV_ERROR){
	    cerr << "Error code " << dwarf_errmsg(error) << endl;
	}

	exit(-1);
    }

    /*
     * Do this setting after init before any real operations.
     * These return the old values, but here we do not
     * neeed to know the old values. The sizes and
     * values here are higher than most ABIs and entirely
     * arbitrary.
     *
     * The setting of initial_value
     * the same as undefined-value (the other possible choice being
     * same-value) is arbitrary, different ABIs do differ, and
     * you have to know which is right.
     *
     * In dwarfdump we get the SAME_VAL, UNDEF_VAL,
     * INITIAL_VAL CFA_VAL from dwconf_s struct.
     * */
    regtabrulecount = 1999;
    dwarf_set_frame_undefined_value(dbg, UNDEF_VAL);
    dwarf_set_frame_rule_initial_value(dbg, INITIAL_VAL);
    dwarf_set_frame_same_value(dbg, SAME_VAL);
    dwarf_set_frame_cfa_value(dbg, CFA_VAL);
    dwarf_set_frame_rule_table_size(dbg, regtabrulecount);
    dwarf_get_address_size(dbg, &_address_size, &error);

    if (_address_size != 32 && _address_size != 64){
	cerr << "Un-supported architecture " << _address_size << endl;
	exit(-1);
    }

    if (!iter_frame(dbg)){
	cerr << "Can't parse eh_frame correctly!" << endl;
    }

    res = dwarf_finish(dbg, &error);
    if (res != DW_DLV_OK){
	cerr << "dwarf_finish failed\n" << endl;
    }

    close(fd);
}

bool FrameParser::get_stack_height(Dwarf_Debug dbg, Dwarf_Fde fde, 
	Dwarf_Addr cur_addr, Dwarf_Error* error, signed& height){
    int res;
    Dwarf_Addr lowpc = 0;
    Dwarf_Unsigned func_length = 0;
    Dwarf_Regtable3 tab3;
    Dwarf_Unsigned fde_byte_length = 0;
    Dwarf_Signed cie_index = 0;
    Dwarf_Off cie_offset = 0;
    Dwarf_Off fde_offset = 0;
    Dwarf_Ptr fde_bytes;
    Dwarf_Half sp_reg = 0;
    Dwarf_Addr actual_pc = 0;
    struct Dwarf_Regtable_Entry3_s* cfa_entry = 0;
    struct Dwarf_Regtable_Entry3_s* sp_entry = 0;

    int oldrulecount = 0;

    res = dwarf_get_fde_range(fde, &lowpc, &func_length, &fde_bytes,
	    &fde_byte_length, &cie_offset, &cie_index, &fde_offset, error);

    if (res != DW_DLV_OK){
	cerr << "Problem getting fde range \n" << endl;
	return false;
    }

    if (cur_addr >= (lowpc + func_length) && cur_addr < lowpc){
	cerr << hex << cur_addr << " does not in current fde!";
	return false;
    }	

    /*
     * 1 is arbitrary. we are winding up getting the rule
     * count here while leaving things unchanged. */
    oldrulecount = dwarf_set_frame_rule_table_size(dbg, 1);
    dwarf_set_frame_rule_table_size(dbg, oldrulecount);

    tab3.rt3_reg_table_size = oldrulecount;
    tab3.rt3_rules = (struct Dwarf_Regtable_Entry3_s *) malloc (
	    sizeof(struct Dwarf_Regtable_Entry3_s) * oldrulecount);

    if (!tab3.rt3_rules){
	cerr << "unable to malloc for " << oldrulecount << " rules" << endl;
	return false;
    }

    res = dwarf_get_fde_info_for_all_regs3(fde, cur_addr, &tab3, &actual_pc, error);

    sp_reg = get_stack_pointer_id();

    if (res != DW_DLV_OK){
	cerr << "dwarf_get_fde_info_for_all_regs3 failed" << endl;
	return false;
    }
    
    if (sp_reg >= tab3.rt3_reg_table_size){
	cerr << "sp(" << sp_reg << ") is bigger than rt3_reg_table_size " 
	    << tab3.rt3_reg_table_size << endl;
	return false;
    }
    cfa_entry = &tab3.rt3_cfa_rule;
    print_one_regentry(cfa_entry);
}

bool FrameParser::print_one_regentry(struct Dwarf_Regtable_Entry3_s *entry){
    Dwarf_Unsigned offset = 0xffffffff;
    Dwarf_Half reg = 0xffff;
#ifdef DWARF_DEBUG
    cout << "type: " << entry->dw_value_type << " " <<
	((entry->dw_value_type == DW_EXPR_OFFSET) ? "DW_EXPR_OFFST" :
	(entry->dw_value_type == DW_EXPR_VAL_OFFSET) ? "DW_EXPR_VAL_OFFSET" : 
	(entry->dw_value_type == DW_EXPR_EXPRESSION) ? "DW_EXPR_EXPRESSION" :
	(entry->dw_value_type == DW_EXPR_VAL_EXPRESSION) ? "DW_EXPR_VAL_EXPRESSION" : "Unknown") << endl;
#endif
    switch(entry->dw_value_type) {
	case DW_EXPR_OFFSET:
	    reg = entry->dw_regnum;
	    if (entry->dw_offset_relevant){
		offset = entry->dw_offset_or_block_len;
	    }
	    break;
	default:
	    cerr << "Can't handle the type " << entry->dw_value_type << " of cfa definition!" << endl;
	    break;
    }

    if (offset == 0xffffffff || reg == 0xffff){
	cerr << "Can't get the register or offset!" << endl;
	return false;
    }

    if (reg != get_stack_pointer_id()){
	cerr << "Wired. CFA is not defined based on stack pointer register(rsp/esp)" << endl;
	return false;
    }
}


bool FrameParser::parse_fde(Dwarf_Debug dbg, Dwarf_Fde fde, Dwarf_Error* error){
    int res;
    Dwarf_Addr lowpc = 0;
    Dwarf_Addr idx_i = 0;
    Dwarf_Unsigned func_length = 0;
    Dwarf_Ptr fde_bytes;
    Dwarf_Unsigned fde_bytes_length = 0;
    Dwarf_Off cie_offset = 0;
    Dwarf_Signed cie_index = 0;
    Dwarf_Off fde_offset = 0;
    Dwarf_Addr arbitrary_addr = 0;
    Dwarf_Addr actual_pc = 0;
    Dwarf_Addr end_func_addr = 0;

    int oldrulecount = 0;
    Dwarf_Ptr outinstrs = 0;
    Dwarf_Unsigned instrslen = 0;
    Dwarf_Frame_Op* frame_op_array = 0;
    Dwarf_Signed frame_op_count = 0;
    Dwarf_Cie cie = 0;

    res = dwarf_get_fde_range(fde, &lowpc, &func_length, &fde_bytes,
	    &fde_bytes_length, &cie_offset, &cie_index, &fde_offset, error);

#ifdef DWARF_DEBUG
    cerr << " FDE: " << hex << lowpc << " -> " << lowpc + func_length << endl; 
#endif

    // TODO. binpang. parse its parent CIE

    if (res != DW_DLV_OK) {
	cerr << "Problem getting fde range " << endl;
	return false;
    }

    res = dwarf_get_fde_instr_bytes(fde, &outinstrs, &instrslen, error);

    if (res != DW_DLV_OK){
	cerr << "dwarf_get_fde_instr_bytes failed!" << endl;
	return false;
    }

    res = dwarf_get_cie_of_fde(fde, &cie, error);

    if (res != DW_DLV_OK){
	cerr << "Error getting cie from fde" << endl;
	return false;
    }

    res = dwarf_expand_frame_instructions(cie, outinstrs, instrslen,
	    &frame_op_array, &frame_op_count, error);

    if (res != DW_DLV_OK){
	cerr << "dwarf_expand_frame_instructions failed!" << endl;
	return false;
    }

    // iter over every instruction, to check every definition of cfas
    if (!check_cfa_def(frame_op_array, frame_op_count)){

#ifdef DWARF_DEBUG
	cerr << "In func " << hex << lowpc <<  ", the definion of cfa is not defined by rsp/esp!" << endl; 
#endif

    }

    dwarf_dealloc(dbg, frame_op_array, DW_DLA_FRAME_BLOCK);
    return true;
}

short unsigned int FrameParser::get_stack_pointer_id(){
    if (_address_size == 32)
	return 4;
    else
	return 7;
}

bool FrameParser::check_cfa_def(Dwarf_Frame_Op* frame_op_array, Dwarf_Signed frame_op_count){
    Dwarf_Signed i = 0;

    bool res = true;

    for (i; i < frame_op_count; ++i){
	Dwarf_Frame_Op *fo = frame_op_array + i;
	switch (fo->fp_extended_op){

	    case DW_CFA_def_cfa:
	    case DW_CFA_def_cfa_sf:
	    case DW_CFA_def_cfa_register:
		// TODO. check if the regiseter is rsp/esp
		//fo->fp_register
		if (_address_size == 64){
		    res = (fo->fp_register != 7) ? false : true;
		} else {
		    res = (fo->fp_register != 4) ? false : true;
		}
		break;
	    
	    // TODO. Handle it. can't handle this now.
	    case DW_CFA_def_cfa_expression:
		res = false;
		break;
	}
    }
    return res;
}

bool FrameParser::iter_frame(Dwarf_Debug dbg){
    Dwarf_Error error;
    Dwarf_Signed cie_element_count = 0;
    Dwarf_Signed fde_element_count = 0;

    Dwarf_Cie *cie_data = 0;
    Dwarf_Fde *fde_data = 0;
    int res = DW_DLV_ERROR;
    Dwarf_Signed fdenum = 0;

    res = dwarf_get_fde_list_eh(dbg, &cie_data, &cie_element_count,
	    &fde_data, &fde_element_count, &error);

    if (res == DW_DLV_NO_ENTRY){
	cerr << "No .eh_frame section!" << endl;
	return false;
    }

    if (res == DW_DLV_ERROR){
	cerr << "Error reading frame data! " << endl;
	return false;
    }

#ifdef DWARF_DEBUG
    cerr << cie_element_count << " cies present. "
	<< fde_element_count << " fdes present. \n" << endl;
#endif

    for (fdenum = 0; fdenum < fde_element_count; ++fdenum){
	Dwarf_Cie cie = 0;

	res = dwarf_get_cie_of_fde(fde_data[fdenum], &cie, &error);

	if (res != DW_DLV_OK) {
	    cerr << "Error accessing cie of fdenum " << fdenum 
		<< " to get its cie" << endl;
	    return false;
	}

#ifdef DWARF_DEBUG
    cerr << " Print cie of fde " << fdenum << endl;

    // parse every fde
    parse_fde(dbg, fde_data[fdenum], &error);

    cerr << " Print fde " << fdenum << endl;
#endif

    }

    dwarf_fde_cie_list_dealloc(dbg, cie_data, cie_element_count,
	    fde_data, fde_element_count);
    return true;
}
