/* file "tcfggen/tcfggen.cpp" */
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

#include <machine/copyright.h>

#ifdef USE_PRAGMA_INTERFACE
#pragma implementation "tcfggen/tcfggen.h"
#endif

#include <machine/machine.h>
#include <suifrm/suifrm.h>
#include <cfa/cfa.h>

#include "tcfggen/tcfggen.h"
#include "tcfggen/lcugen.h"
#ifdef USE_DMALLOC
#include <dmalloc.h>
#define new D_NEW
#endif

#define KEEP         0
#define CONVERT_NOP  1
#define REMOVE       2


void lcugen(NaturalLoopInfo nlinfo, Cfg *cfg_in);
void sprint_data_task(char *outstr, int i);
int get_loop_initialization_bb_num(task_data task_data_arr_in[100], unsigned int loop_num, unsigned int num_tasks);
int get_max_loop_num(task_data task_data_arr_in[100], unsigned int num_tasks);
int get_max_task_id(task_data task_data_arr_in[100], unsigned int num_tasks);
int get_bb_task_num(task_data task_data_arr_in[100], unsigned int bb_num, unsigned int num_tasks);


extern task_data task_data_arr[100];
extern tcfg_edge TCFG[100];
extern cfg_instr_pos TaskEntryInstr[100],TaskExitInstr[100];
extern cfg_instr_pos LoopOverheadInstr[100];
extern unsigned node_end_arr[100];
extern unsigned edge_list_max;        // number of unique task transition entries
extern unsigned i_max;                // number of tasks
extern unsigned cac_task_id_max;      // number of (redundant) task transition entries
extern IdString k_lix, k_dpt, k_dptt, k_loop, k_overhead;

const char *cur_proc_name;
char *copied_cur_proc_name;
bool gen_lut_file_g, gen_vcg_file_g, gen_fsm_file_g, gen_cac_file_g;


class LoopIndexNote : public Note {
 public:
    LoopIndexNote() : Note(note_list_any()) { }
    LoopIndexNote(const LoopIndexNote &other) : Note(other) { }
    LoopIndexNote(const Note &note) : Note(note) { }

    int get_ixnum() const               { return _get_c_long(0); }
    void set_ixnum(int ixnum)           { _replace(0, ixnum); }
    IdString get_ixnum_str() const      { return _get_string(1); }
    void set_ixnum_str(IdString lix_id) { _replace(1, lix_id); }
    int get_task_id() const             { return _get_c_long(2); }
    void set_task_id(int task_id)       { _replace(2, task_id); }
    int get_ttsel() const               { return _get_c_long(3); }
    void set_ttsel(int ttsel)           { _replace(3, ttsel); }
    int get_fwdsel() const              { return _get_c_long(4); }
    void set_fwdsel(int fwdsel)         { _replace(4, fwdsel); }
    int get_loop_addr() const           { return _get_c_long(5); }
    void set_loop_addr(int loop_addr)   { _replace(5, loop_addr); }
    IdString get_task_enc() const       { return _get_string(6); }
    void set_task_enc(IdString task_id) { _replace(6, task_id); }
};

/*
 * DpttNote = data-processing task transition note
 * Attach to the last instruction in a task
 */
class DpttNote : public Note {
 public:
    DpttNote() : Note(note_list_any()) { }
    DpttNote(const DpttNote &other) : Note(other) { }
    DpttNote(const Note &note) : Note(note) { }

    int get_id() const                     { return _get_c_long(0); }
    void set_id(int id)                    { _replace(0, id); }
    int get_current_taskid() const         { return _get_c_long(1); }
    void set_current_taskid(int taskid)    { _replace(1, taskid); }
    int get_next_taskid() const            { return _get_c_long(2); }
    void set_next_taskid(int taskid)       { _replace(2, taskid); }
    int get_next_ttsel() const             { return _get_c_long(3); }
    void set_next_ttsel(int ttsel)         { _replace(3, ttsel); }
    int get_next_loop_addr() const         { return _get_c_long(4); }
    void set_next_loop_addr(int loop_addr) { _replace(4, loop_addr); }
};

/*
 * DptNote = data-processing task note
 * Attach to the first instruction in the data-processing task
 */
class DptNote : public Note {
 public:
    DptNote() : Note(note_list_any()) { }
    DptNote(const DptNote &other) : Note(other) { }
    DptNote(const Note &note) : Note(note) { }

    int get_id() const                     { return _get_c_long(0); }
    void set_id(int id)                    { _replace(0, id); }
    int get_first_bb() const               { return _get_c_long(1); }
    void set_first_bb(int bb_num)          { _replace(1, bb_num); }
    int get_last_bb() const                { return _get_c_long(2); }
    void set_last_bb(int bb_num)           { _replace(2, bb_num); }
};

/*
 * LoopNote = loop note
 * Attach to the last instruction (should be a BLT) in a bwd task
 */
class LoopNote : public Note {
 public:
    LoopNote() : Note(note_list_any()) { }
    LoopNote(const LoopNote &other) : Note(other) { }
    LoopNote(const Note &note) : Note(note) { }

    int get_loop_addr() const           { return _get_c_long(0); }
    void set_loop_addr(int loop_addr)   { _replace(0, loop_addr); }
    int get_ixnum() const               { return _get_c_long(1); }
    void set_ixnum(int ixnum)           { _replace(1, ixnum); }
    int get_initial() const             { return _get_c_long(2); }
    void set_initial(int initial)       { _replace(2, initial); }
    int get_step() const                { return _get_c_long(3); }
    void set_step(int step)             { _replace(3, step); }
    int get_final() const               { return _get_c_long(4); }
    void set_final(int final)           { _replace(4, final); }
};

/*
 * OvrhdInstrNote = overhead instruction note (either to remove or to replace by NOP)
 * Attach to the specified instruction
 */
class OvrhdInstrNote : public Note {
 public:
    OvrhdInstrNote() : Note(note_list_any()) { }
    OvrhdInstrNote(const OvrhdInstrNote &other) : Note(other) { }
    OvrhdInstrNote(const Note &note) : Note(note) { }

    int get_istate() const                { return _get_c_long(0); }
    void set_istate(int istate)           { _replace(0, istate); }
};


void TcfgGen::do_opt_unit(OptUnit *unit)
{
    OptUnit *cur_unit;
    FILE *loop_report;
    static int procedure_count = 0;

    // Make local copy of the optimization unit
    cur_unit = unit;

    // Report name of the CFG under processing
    cur_proc_name = get_name(cur_unit).chars();
    dbg_printf("Processing CFG \"%s\"\n", cur_proc_name);
    copied_cur_proc_name = (char *)malloc(48*sizeof(char *));
    strcpy(copied_cur_proc_name,cur_proc_name);

    // Get the body of the OptUnit
    AnyBody *cur_body = get_body(cur_unit);
//    AnyBody *cur_body = get_body(unit);

    // verify that it is a CFG
    claim(is_kind_of<Cfg>(cur_body), "expected OptUnit body in Cfg form");

    // Open loop analysis report file
    if (procedure_count==0)
    {
      loop_report = fopen("loop_results.txt","w");
    }
    else
      loop_report = fopen("loop_results.txt","a");
      
    gen_lut_file_g = gen_lut_file;
    gen_vcg_file_g = gen_vcg_file;
    gen_fsm_file_g = gen_fsm_file;
    gen_cac_file_g = gen_cac_file;

    // Create a local copy of the input CFG
    Cfg *cfg = (Cfg *)cur_body;

    // Add a NOP to empty basic blocks
    for (CfgNodeHandle cfg_nh=nodes_start(cfg); cfg_nh!=nodes_end(cfg); ++cfg_nh)
    {
      // Get the current node
      CfgNode* cnode = get_node(cfg, cfg_nh);
//      Instr *mi_first;

      if (size(cnode) == 0)
      {
	// Append a NOP as the first instruction in the BB
	append(cnode, new_instr_alm(suifrm::NOP));
      }
    }

    // Simplify the CFG
    while (
      remove_unreachable_nodes(cfg) ||
      merge_node_sequences(cfg) ||
      optimize_jumps(cfg)
    )
    {
    }

    canonicalize(cfg);

    DominanceInfo temp_dom(cfg);

    // Generate dominance info
    temp_dom.find_dominators();
    temp_dom.print(loop_report);

    //NaturalLoopInfo temp_lnat(temp_dom) : dom_info(temp_dom), _depth(NULL), _loop(NULL);
    NaturalLoopInfo temp_lnat(&temp_dom);

    // Generate natural loop info
    temp_lnat.find_natural_loops();

    // Print natural loop info
    temp_lnat.print(loop_report);

    lcugen(temp_lnat, cfg);

    // Identify a looping instruction pattern in the current instruction list
    // NOTE: Currently, only looking for an add-ldc-blt pattern
    //

    Opnd rix,rfinal;
    int  cinitial,cstep,cfinal;
    bool is_looping_pattern_flag = false;
    bool is_looping_pattern_flag_arr[250];
    bool is_loop_add=false,is_loop_ldc=false,is_loop_blt=false;
    int loop_index_arr[100],loop_initial_arr[100],loop_step_arr[100],loop_final_arr[100];
    int LoopOverheadInstr_id=0;

  loop_index_arr[0] = 0;
  loop_initial_arr[0] = 0;
  loop_step_arr[0] = 1;
  loop_final_arr[0] = 0;

  // Iterate through the nodes of the CFG
  for (CfgNodeHandle cfg_nh=nodes_start(cfg); cfg_nh!=nodes_end(cfg); ++cfg_nh)
  {
    // Get the current node
    CfgNode* cnode = get_node(cfg, cfg_nh);
    int local_loop_addr = task_data_arr[get_bb_task_num(task_data_arr,get_number(cnode),i_max)].loop_addr;

    // if this is a loop-end and loop-exit CFG node (BB) then
    // it must contain the loop overhead instruction pattern
    // NOTE: This seems valid for well-structured (and optimized) SUIFvm code

    if (temp_lnat.is_loop_end(cnode) && temp_lnat.is_loop_exit(cnode))
    {
      dbg_printf("BB #%d should contain a loop overhead instruction pattern\n",get_number(cnode));

      // Iterate through CfgNode cnode (current basic block)
      InstrHandle hk = instrs_start(cnode);
      //
      while (hk != instrs_end(cnode))
//      for (InstrHandle hk = instrs_start(cnode); hk != instrs_end(cnode); ++hk)
      {
        Instr *mk = *hk;

        // If mk is a BLT instruction (possibly closing a loop)
	// NOTE: This is a naive approach. Real "induction variable identification" is far
	// more complex and cannot be based on "pattern matching".
	//
	if (get_opcode(mk) == suifrm::BLT)
	{
	  dbg_printf("Found a BLT in the looping pattern\n");
	  is_loop_blt = true;

	  // Get src0 operand of BLT
	  rix = get_src(mk, 0);

	  // Get src1 operand of BLT
	  rfinal = get_src(mk, 1);

	  // Access the predecessing instruction in cnode
	  --hk;
	  Instr *ml = *hk;

	  if (get_opcode(ml) == suifrm::LDC &&
	      (get_dst(ml, 0) == rfinal))
	  {
	    dbg_printf("Found an LDC in the looping pattern\n");
	    is_loop_ldc = true;

	    // Access the predecessing instruction in cnode
	    --hk;
	    Instr *mi = *hk;

	    // Access cfinal immed operand
	    if (is_immed_integer(get_src(ml, 0)))
	    {
	      cfinal = get_immed_int(get_src(ml, 0));

	      if (is_loop_blt)
	        loop_final_arr[local_loop_addr] = cfinal-1;
	      else //if (is_loop_ble)
	        loop_final_arr[local_loop_addr] = cfinal;
	    }

	    if (get_opcode(mi) == suifrm::ADD &&
	        (get_dst(mi, 0) == rix) &&
	        (get_src(mi, 0) == rix))
	    {
	      dbg_printf("Found an ADD in the looping pattern\n");
	      is_loop_add = true;

	      // Access cstep immed operand
	      if (is_immed_integer(get_src(mi, 1)))
	      {
	        cstep = get_immed_int(get_src(mi, 1));
	        loop_step_arr[local_loop_addr] = cstep;
	      }

	      hk++;
	    }

	    hk++;
	  }
	}

	hk++;
      }
    }

    if (is_loop_blt && is_loop_ldc && is_loop_add)
    {
//      remove(cfg_nh);

      is_looping_pattern_flag = true;

//      lix_note.set_ixnum_str(IdString( reg_name(get_reg(rix)) ));
//      lix_note.set_ixnum(ix_id);
//      int local_loop_addr = task_data_arr[get_bb_task_num(task_data_arr,get_number(cnode),i_max)].loop_addr;
      loop_index_arr[local_loop_addr] = get_reg(rix);

//      fprintf(stdout,"Identified operand ");
//      fprint(stdout,rix);
//      fprintf(stdout," as loop index variable %d\n", local_loop_addr);

      int cnode_num = get_number(cnode);
      int should_remove_blt=0;

      // Find out if the current task (which as we already know features a looping pattern)
      // consists solely of overhead instructions. If this is the case, the BLT should be
      // replaced by a NOP.
      for (unsigned int i=0; i<i_max; i++)
      {
        // if cnode is contained in this task
        if (task_data_arr[i].bb_list[0] <= cnode_num && task_data_arr[i].bb_list[task_data_arr[i].bb_list_size-1] >= cnode_num)
	{
	  if (task_data_arr[i].bb_list[0] != task_data_arr[i].bb_list[task_data_arr[i].bb_list_size-1])
	  {
	    should_remove_blt = 1;
	    break;
	  }
	}
      }

      // Update LoopOverheadInstr entry for this bwd task BB
      // if task_size > 3 then cti -> REMOVE else cti -> CONVERT
//      if (size(cnode) > 3+1)
      if (should_remove_blt == 1)
      {
        LoopOverheadInstr[LoopOverheadInstr_id].bb_num = get_number(cnode);
        LoopOverheadInstr[LoopOverheadInstr_id].instr_num = size(cnode)-1;
        LoopOverheadInstr[LoopOverheadInstr_id].istate = REMOVE;
      }
      else
      {
        LoopOverheadInstr[LoopOverheadInstr_id].bb_num = get_number(cnode);
        LoopOverheadInstr[LoopOverheadInstr_id].instr_num = size(cnode)-1;
        LoopOverheadInstr[LoopOverheadInstr_id].istate = CONVERT_NOP;
      }
      LoopOverheadInstr_id++;
      // add -> REMOVE
      //{
        LoopOverheadInstr[LoopOverheadInstr_id].bb_num = get_number(cnode);
        LoopOverheadInstr[LoopOverheadInstr_id].instr_num = size(cnode)-2;
        LoopOverheadInstr[LoopOverheadInstr_id].istate = REMOVE;
      //}
      LoopOverheadInstr_id++;

      // ldc -> REMOVE
      //{
        LoopOverheadInstr[LoopOverheadInstr_id].bb_num = get_number(cnode);
        LoopOverheadInstr[LoopOverheadInstr_id].instr_num = size(cnode)-3;
        LoopOverheadInstr[LoopOverheadInstr_id].istate = REMOVE;
      //}
      LoopOverheadInstr_id++;
    }

    is_looping_pattern_flag_arr[get_number(cnode)] = is_looping_pattern_flag;
    is_looping_pattern_flag = false;

    is_loop_add = false;
    is_loop_ldc = false;
    is_loop_blt = false;

  }

  // Iterate through the nodes of the CFG
  for (CfgNodeHandle cfg_nh=nodes_start(cfg); cfg_nh!=nodes_end(cfg); ++cfg_nh)
  {
    // Get the current node
    CfgNode* cnode = get_node(cfg, cfg_nh);

    unsigned int i,j;

      for (i=0; i<i_max; i++)
      {
        for (j=0; j<task_data_arr[i].bb_list_size; j++)
	{
	  if (task_data_arr[i].bb_list[j] == get_number(cnode))
	  {
            LoopIndexNote lix_note;

            lix_note.set_ixnum(loop_index_arr[task_data_arr[i].loop_addr]);
	    lix_note.set_task_id(task_data_arr[i].taskid);
	    lix_note.set_ttsel(task_data_arr[i].FSMsel);
	    lix_note.set_fwdsel(task_data_arr[i].fwdsel);
	    lix_note.set_loop_addr(task_data_arr[i].loop_addr);

	    char *task_enc_str = new char[20];
	    sprint_data_task(task_enc_str, i);
	    lix_note.set_task_enc(IdString( task_enc_str ));

            set_note(cnode, k_lix, lix_note);
	  }
	}
      }
  }

  // Identify the cinitial constant (loop initial parameter) for each loop in
  // the given CFG
  for (CfgNodeHandle cfg_nh=nodes_start(cfg); cfg_nh!=nodes_end(cfg); ++cfg_nh)
  {
    // Get the current node
    CfgNode* cnode = get_node(cfg, cfg_nh);
    int cnode_num = get_number(cnode);

    // Examine note
    if (has_note(cnode, k_lix))
    {
    LoopIndexNote lix_note_read = get_note(cnode, k_lix);
    claim(!is_null(lix_note_read));

//    fprintf(stdout,"ttsel=%d\ttask_id=%d\tnode_end=%d\n",
//    lix_note_read.get_ttsel(), lix_note_read.get_task_id(), node_end_arr[cnode_num]);

    // if this is the end and exit block of a bwd task
    if (lix_note_read.get_ttsel() == 0 &&
        node_end_arr[cnode_num] == 1)
    {
      int cnode_loopinit_num = get_loop_initialization_bb_num(task_data_arr, lix_note_read.get_loop_addr(), i_max);

      dbg_printf("BB #%d contains the loop initialization of loop_addr %d\n",
      cnode_loopinit_num, lix_note_read.get_loop_addr());

      CfgNode* cnode_loopinit = get_node(cfg, cnode_loopinit_num);

      int m=0;

      // Iterate through CfgNode cnode (current basic block)
      for (InstrHandle hk = instrs_start(cnode_loopinit); hk != instrs_end(cnode_loopinit); ++hk)
      {
        Instr *mk = *hk;

        // If mk is a LDC (load constant) instruction and the dest register is the
	// loop index register, then the loaded constant is cinitial
	//
	if (get_opcode(mk) == suifrm::LDC)
	{
	  // Get cinitial candidate operand
	  claim(is_immed_integer(get_src(mk, 0)));
	  cinitial = get_immed_int(get_src(mk, 0));

          dbg_printf("ixnum=%d\tldc_dst0=%d\n",lix_note_read.get_ixnum(),get_reg(get_dst(mk,0)));

      	  // check if the destination register of the LDC is the loop index register
	  if (lix_note_read.get_ixnum() == get_reg(get_dst(mk, 0)))
	  {
	    dbg_printf("Found a loop initialization pattern\n");

	    loop_initial_arr[lix_note_read.get_loop_addr()] = cinitial;

            // ldc -> REMOVE
            {
              LoopOverheadInstr[LoopOverheadInstr_id].bb_num = get_number(cnode_loopinit);
              LoopOverheadInstr[LoopOverheadInstr_id].instr_num = m;
              LoopOverheadInstr[LoopOverheadInstr_id].istate = REMOVE;
            }
            LoopOverheadInstr_id++;
	  }
	}

	m++;
      }
    }
    }
  }

  dbg_printf("\n");

  // Iterate through the nodes of the CFG
  for (CfgNodeHandle cfg_nh=nodes_start(cfg); cfg_nh!=nodes_end(cfg); ++cfg_nh)
  {
    // Get the current node
    CfgNode* cnode = get_node(cfg, cfg_nh);

    // Examine note
    if (has_note(cnode, k_lix))
    {
    LoopIndexNote lix_note_read = get_note(cnode, k_lix);
    claim(!is_null(lix_note_read));

    // dptt <task-enc> (<dpt-entry>), <task-id>, <ttsel>, <loop-addr>,<fwd-sel>
    dbg_printf("LoopInfoNote\t%s (%d), %d, %d, %d, v%d # @ BB%d\n",
            lix_note_read.get_task_enc().chars(),
	    lix_note_read.get_task_id(),
	    lix_note_read.get_ttsel(),
	    lix_note_read.get_loop_addr(),
	    lix_note_read.get_fwdsel(),
	    lix_note_read.get_ixnum(),
	    get_number(cnode));
    }
  }

  for (unsigned int i=0; i<i_max; i++)
  {
    // dpti <entry-num>, <first-bb-num>, <last-bb-num>
    dbg_printf(".dpti\t%d, %d, %d\n",
            i,
            task_data_arr[i].bb_list[0],
	    task_data_arr[i].bb_list[task_data_arr[i].bb_list_size-1]);
  }

  // Attach a DptNote to the first instruction of each data-processing task
  for (CfgNodeHandle cfg_nh=nodes_start(cfg); cfg_nh!=nodes_end(cfg); ++cfg_nh)
  {
    // Get the current node
    CfgNode* cnode = get_node(cfg, cfg_nh);
    int cnode_num = get_number(cnode);

    for (unsigned int i=0; i<i_max; i++)
    {
      if (task_data_arr[i].bb_list[0] == cnode_num)
      {
	// Create the note
	DptNote dpt_note;

	dpt_note.set_id(i);
	dpt_note.set_first_bb(task_data_arr[i].bb_list[0]);
	dpt_note.set_last_bb(task_data_arr[i].bb_list[task_data_arr[i].bb_list_size-1]);

        // Get the 1st instruction in the basic block (BB = CfgNode)
        Instr *mi_first_nonlabel = first_non_label(cnode);

	set_note(mi_first_nonlabel, k_dpt, dpt_note);

	// Examine note
	DptNote dpt_note_read = get_note(mi_first_nonlabel, k_dpt);
      }
    }
  }

  for (unsigned int i=0; i<cac_task_id_max; i++)
  {
    // ldst <entry-num>, <current-task-data>, <next-task-data>, <next-ttsel>, <next-loop-addr>
    dbg_printf(".dptt\t%d, %d, %d, %d, %d\n",
            i,
            TCFG[i].current_taskid,
	    TCFG[i].next_taskid,
	    TCFG[i].next_ttsel,
	    TCFG[i].next_loop_addr);
  }

  for (unsigned int i=0; i<cac_task_id_max; i++)
  {
    // Get the task ID
    int local_taskid = TCFG[i].current_taskid;

    // Get the ID for the last BB in the specified task
    int last_bb_num = task_data_arr[local_taskid].bb_list[task_data_arr[local_taskid].bb_list_size-1];

    // Get the corresponding CfgNode
    CfgNode* cnode = get_node(cfg, last_bb_num);

    Instr *mi_last_noncti;

    DpttNote dptt_note;

    dptt_note.set_id(i);
    dptt_note.set_current_taskid(TCFG[i].current_taskid);
    dptt_note.set_next_taskid(TCFG[i].next_taskid);
    dptt_note.set_next_ttsel(TCFG[i].next_ttsel);
    dptt_note.set_next_loop_addr(TCFG[i].next_loop_addr);

    // Get the last non-cti instruction in the basic block (BB = CfgNode)
    mi_last_noncti = last_non_cti(cnode);

    set_note(mi_last_noncti, k_dptt, dptt_note);

    // Examine note
    DpttNote dptt_note_read = get_note(mi_last_noncti, k_dptt);
  }

  for (int i=0; i<=get_max_loop_num(task_data_arr, i_max); i++)
  {
    dbg_printf(".loop\t%d, %d, %d, %d, %d\n",
            i,
	    loop_index_arr[i],
	    loop_initial_arr[i],
	    loop_step_arr[i],
	    loop_final_arr[i]);
  }

//  int loop_id = 0;

  // Iterate through the nodes of the CFG
  for (CfgNodeHandle cfg_nh=nodes_start(cfg); cfg_nh!=nodes_end(cfg); ++cfg_nh)
  {
    // Get the current node
    CfgNode* cnode = get_node(cfg, cfg_nh);
    int cnode_num = get_number(cnode);

    // Examine note
    if (has_note(cnode, k_lix))
    {
    LoopIndexNote lix_note_read = get_note(cnode, k_lix);

    claim(!is_null(lix_note_read));

    // Check if this BB contains the add-cmp-branch pattern of a bwd task
    // Actually read the task_id and ttsel fields of the LoopIndexNote that is
    // already attached to this BB
    int local_task_id = lix_note_read.get_task_id();
    int local_ttsel   = lix_note_read.get_ttsel();
    int loop_id       = lix_note_read.get_loop_addr();

    Instr *mi_last_noncti;

    if (local_ttsel == 0)
    {
      // Get the id of the last BB in the task
      int last_bb_num = task_data_arr[local_task_id].bb_list[task_data_arr[local_task_id].bb_list_size-1];

      if (last_bb_num == cnode_num)
      {
	LoopNote loop_note;

	loop_note.set_loop_addr(loop_id);
	loop_note.set_ixnum(loop_index_arr[loop_id]);
	loop_note.set_initial(loop_initial_arr[loop_id]);
	loop_note.set_step(loop_step_arr[loop_id]);
	loop_note.set_final(loop_final_arr[loop_id]);

        // Get the last non-cti instruction in the basic block (BB = CfgNode)
	mi_last_noncti = last_non_cti(cnode);

	set_note(mi_last_noncti, k_loop, loop_note);

	// Examine note
	LoopNote loop_note_read = get_note(mi_last_noncti, k_loop);
      }
    }
    }
  }

  for (int i=0; i<LoopOverheadInstr_id; i++)
  {
    dbg_printf(".overhead\t%d, %d, %d, %d\n",
            i,
	    LoopOverheadInstr[i].bb_num,
	    LoopOverheadInstr[i].instr_num,
	    LoopOverheadInstr[i].istate);
  }

  for (int i=0; i<LoopOverheadInstr_id; i++)
  {
    // Get the BB id for the specified overhead instruction
    int cnode_num = LoopOverheadInstr[i].bb_num;

    // Get the BB
    CfgNode* cnode = get_node(cfg, cnode_num);

    if (LoopOverheadInstr[i].bb_num == (unsigned)cnode_num)
    {
      unsigned int hk_num=0;

      // Iterate through CfgNode cnode (current basic block)
      for (InstrHandle hk = instrs_start(cnode); hk != instrs_end(cnode); ++hk)
      {
        Instr *mk = *hk;

	if (LoopOverheadInstr[i].instr_num == hk_num)
	{
	  OvrhdInstrNote ovhi_note;

	  ovhi_note.set_istate(LoopOverheadInstr[i].istate);

	  set_note(mk, k_overhead, ovhi_note);

	  // Examine note
	  OvrhdInstrNote ovhi_note_read = get_note(mk, k_overhead);
	}

	hk_num++;
      }
    }
  }


  procedure_count++;
}   /*** END OF tcfggen.cpp */
