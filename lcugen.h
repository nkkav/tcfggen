/* file "tcfggen/tcfggen.h" */
/*
 *     Copyright (c) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
 *                   2011, 2012, 2013, 2014, 2015, 2016 Nikolaos Kavvadias
 *
 *     This software was written by Nikolaos Kavvadias, Ph.D. candidate
 *     at the Physics Department, Aristotle University of Thessaloniki,
 *     Greece (at the time).
 *
 *     This software is provided under the terms described in
 *     the "machine/copyright.h" include file.
 */

#ifndef TCFGGEN_LCUGEN_H
#define TCFGGEN_LCUGEN_H

#include <machine/copyright.h>

#ifdef USE_PRAGMA_INTERFACE
#pragma interface "tcfggen/lcugen.h"
#endif

#include <machine/machine.h>


typedef struct task_data_t task_data;
typedef struct address_t address;
typedef struct data_t data;
typedef struct task_trans_t task_trans;
typedef struct cfg_instr_pos_t cfg_instr_pos;
typedef struct tcfg_edge_t tcfg_edge;

/*typedef*/ struct task_data_t
{
    unsigned node_begin;
	unsigned node_end;
	unsigned taskid;
	int      FSMsel;
	unsigned fwdsel;
	unsigned loop_addr;
	unsigned inner_loop;
	unsigned annotation;
	int      bb_list[40];
	unsigned bb_list_size;
}/* task_data*/;

/*typedef*/ struct address_t
{
	unsigned gloop_end;
	int FSMsel;
	unsigned fwdsel;
	unsigned loop_addr;
}/* address*/;

/*typedef*/ struct data_t
{
	int FSMsel;
	unsigned fwdsel;
	unsigned loop_addr;
}/* data*/;

/*typedef*/ struct task_trans_t
{
	address LUT_address;
	data LUT_data;
}/* task_trans*/;

/*typedef*/ struct cfg_instr_pos_t
{
        unsigned bb_num;
	unsigned instr_num;
	unsigned istate;     // State of the marked instr: KEEP (0), CONVERT_NOP (1), REMOVE (2)
}/* cfg_instr_pos*/;

/*typedef*/ struct tcfg_edge_t
{
        unsigned current_taskid;
	unsigned next_taskid;
	unsigned next_ttsel;
	unsigned next_loop_addr;
}/* tcfg_edge*/;


#endif /* TCFGGEN_LCUGEN_H */
