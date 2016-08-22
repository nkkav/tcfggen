/* file "tcfggen/lcugen.cpp" */
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
/* Description: A simple pass on the natural loop analysis results.
 *              It is used to assign proper addresses to the data processing
 *              tasks of the algorithm under investigation.
 */
/* Author: Nikos Kavvadias, <nkavv@physics.auth.gr> */

/* EXAMPLE LOOP ANALYSIS RESULTS */
/*
Loop info:
  node depth begin end exit
   int: int   Y/N  Y/N  Y/N
   ...................................
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include <machine/copyright.h>

#ifdef USE_PRAGMA_INTERFACE
#pragma implementation "tcfggen/tcfggen.h"
#endif

#include <machine/machine.h>
#include <suifrm/suifrm.h>
#include <cfa/cfa.h>
#include <cfa/dom.h>
#include <cfa/loop.h>

#include "tcfggen/tcfggen.h"
#include "tcfggen/lcugen.h"
#include "tcfggen/suif_pass.h"

#ifdef USE_DMALLOC
#include <dmalloc.h>
#define new D_NEW
#endif


#define PRINT_DEBUG

#define FALSE 0
#define TRUE  1

#define L_OPEN  0
#define L_CLOSE 1

#define BWD     0
#define FWD     1

#define TAIL    0
#define HEAD    1
#define WEIGHT  2

#define KEEP         0
#define CONVERT_NOP  1
#define REMOVE       2


typedef struct node
{
	unsigned data;
	struct node *link;
} LNODE;


// FUNCTION PROTOTYPES
int get_bb_task_num(task_data task_data_arr_in[100], unsigned int bb_num, unsigned int num_tasks);
int get_loop_initialization_bb_num(task_data task_data_arr_in[100], unsigned int loop_num, unsigned int num_tasks);
int get_max_loop_num(task_data task_data_arr_in[100], unsigned int num_tasks);
int get_max_task_id(task_data task_data_arr_in[100], unsigned int num_tasks);
unsigned bool2uint(bool bool_val);
unsigned translate_char(char c);
unsigned get_item(LNODE *ptr);
void show_item(LNODE *ptr);
void show_stack(LNODE *ptr);
unsigned empty(LNODE *ptr);
void push(LNODE **ptr, unsigned item);
void pop(LNODE **ptr, unsigned *item);
unsigned dectobin(unsigned bin_data, int num_bits);
unsigned ipow(unsigned x, unsigned y);
unsigned log2(unsigned operand);
void write_file_lut(FILE *outfile);
void write_file_fsm(FILE *outfile);
void write_file_cac(FILE *outfile);
void generate_tcfg_entries(task_data task_data_arr_in[100]);
unsigned int lsr_by_1(unsigned int val);
void print_rom_data(FILE *outfile, int i);
void itob(unsigned i, char *s, int num_bits);
void write_file_vcg(FILE *outfile);
void sprint_data_task(char *outstr, int i);
void print_data_task(FILE *outfile, int i);
void print_data_task_fsm(FILE *outfile, int i);
void remove_task_data(int i);
void assign_edges(int j);
void remove_common_edges(int j);
void rearrange_edge_list(int j);
void load_annot_file();
void generate_graph();
void print_weight(FILE *outfile, int i);
void init_task_data_arr();


task_data task_data_arr[100];
task_data task_annot_arr[100];
task_trans LUT_contents[100];
tcfg_edge TCFG[100];
cfg_instr_pos TaskEntryInstr[100],TaskExitInstr[100]; // these are addressed by task_id
cfg_instr_pos LoopOverheadInstr[100]; // this is selectively accessed by BBs containing either
                                      // loop index initialization (REMOVE), and loop overhead
				      // instructions (add, cmp -> REMOVE, cti -> NOP if bb_size==3
				      // else cti -> REMOVE))
unsigned node_num_arr[100], loop_depth_arr[100], node_begin_arr[100], node_end_arr[100], node_exit_arr[100];
int edge_list[100][3];
//
extern bool gen_lut_file_g, gen_vcg_file_g, gen_fsm_file_g, gen_cac_file_g;
extern char *copied_cur_proc_name;
//
FILE *file_lut;             /* If -lut option is specified, the VHDL source for the
                             * control unit LUT is generated.
                             */
FILE *file_vcg;             /* If -vcg option is specified, the VCG for the task
                             * graph of the algorithm is generated.
                             */
FILE *file_fsm;             /* If -fsm option is specified, the VHDL source for the
                             * control unit FSM is generated.
                             */
FILE *file_cac;             /* If -cac option is specified, the C source for
                             * initialization of the task selection unit is generated.
                             */

char vcg_file_name[32], loop_analysis_res_name[32];
char annot_file_name[32];
char lut_file_name[32], fsm_file_name[32], cac_file_name[32];
unsigned i_max;
unsigned task_trans_max, edge_list_max, cac_task_id_max;
unsigned fwdsel_max, nlp;
time_t t;


void lcugen(NaturalLoopInfo nlinfo, Cfg *cfg_in)
{
  unsigned size;
  unsigned size_max = 0;

  int cnode_num;

  unsigned i, j/*, m*/;
  unsigned loop_addr_max = 0;

  LNODE *stacka = NULL;
  unsigned loop_addr_i = 0, bbl_size = 0;


  // Parse loop analysis results
  // Iterate through the nodes of the CFG
  for (CfgNodeHandle cfg_nh=nodes_start(cfg_in); cfg_nh!=nodes_end(cfg_in); ++cfg_nh)
  {
    // Get the current node
    CfgNode* cnode = get_node(cfg_in, cfg_nh);
    cnode_num = get_number(cnode);

    node_num_arr[cnode_num]   = cnode_num;
    loop_depth_arr[cnode_num] = nlinfo.loop_depth(cnode_num);
    node_begin_arr[cnode_num] = bool2uint(nlinfo.is_loop_begin(cnode_num));
    node_end_arr[cnode_num]   = bool2uint(nlinfo.is_loop_end(cnode_num));
    node_exit_arr[cnode_num]  = bool2uint(nlinfo.is_loop_exit(cnode_num));

    dbg_printf("BB %d: %d %d %d %d\n",
    node_num_arr[cnode_num],loop_depth_arr[cnode_num],node_begin_arr[cnode_num],node_end_arr[cnode_num],node_exit_arr[cnode_num]);
  }


  // Initialize task_data_arr[] entries to ZERO
  init_task_data_arr();

  size_max = cnode_num;

  // Initialize loop_addr entries
  for (size=0; size<size_max; size++)
    task_data_arr[size].loop_addr = 0;

  // Initialize taskid entries
  for (size=0; size<size_max; size++)
    task_data_arr[size].taskid = 0;

  // Initialize FSMsel (task type) entries
  for (size=0; size<size_max; size++)
    task_data_arr[size].FSMsel = -1;

  size = 0;
  i = 0;
  loop_addr_i = 0;
  //
  // Count fwd0(0) task
  task_data_arr[i].node_begin = size;
  task_data_arr[i].FSMsel = 1;
  task_data_arr[i].fwdsel = 0;
  task_data_arr[i].loop_addr = loop_addr_i;
  task_data_arr[i].taskid = i;
  i++;

  size = 1;
  //
  while (size < size_max)
  {
    // If it is a loop_begin node
    if (node_begin_arr[size] == 1)
    {
      // Increment loop_addr, loop_addr_max
      loop_addr_i++;
      loop_addr_max++;
      //
      // Then push loop into loop stack
      push(&stacka, loop_addr_max);
    }

    // If it is not a loop_end node
    if (node_end_arr[size] != 1)
    {
      // If it is a loop_begin node
      if (node_begin_arr[size] == 1)
      {
        task_data_arr[i].node_begin = size;

	// Loop address is at top-of-stack
	task_data_arr[i].loop_addr = loop_addr_max;
	//
	// Prepare for next entry in task_data array
	task_data_arr[i].taskid = i;
	i++;
      }
//      else
//      {
//      }
    }
//    else
//    {
//    }

    // If it is a loop_end node
    if (node_end_arr[size] == 1)
    {
      task_data_arr[i].node_begin = size+1;
      task_data_arr[i].FSMsel = 0;
      task_data_arr[i].fwdsel = 0;

      // Then pop loop from the loop stack
      pop(&stacka, &loop_addr_i);

      // Current loop address is at top-of-stack
      task_data_arr[i].loop_addr = get_item(stacka);

      // Prepare for next entry in task_data array
      task_data_arr[i].taskid = i;
      i++;
    }
//    else
//    {
//    }

    // Goto to following node
    size++;
  }


  i_max = i;

  // Identify basic block list belonging to each task
  size = 0;
  i = 0;
  bbl_size = 0;
  dbg_printf("BB %d belongs to identified task %d\n", size, i);
  task_data_arr[i].bb_list[bbl_size] = size;
  bbl_size++;
  //
  size++;
  //
  while (size < size_max)
  {
    if (loop_depth_arr[size] != loop_depth_arr[size-1])
    {
      task_data_arr[i].bb_list_size = bbl_size;
      bbl_size = 0;
      i++;
    }

    dbg_printf("BB %d belongs to identified task %d\n", size, i);
    task_data_arr[i].bb_list[bbl_size] = size;
    bbl_size++;

    size++;
  }
  //
  dbg_printf("BB %d belongs to identified task %d\n", size, i);
  task_data_arr[i].bb_list[bbl_size] = size;
  bbl_size++;
  task_data_arr[i].bb_list_size = bbl_size;


  for (j=0; j<i_max; j++)
    task_data_arr[j].node_end = task_data_arr[j+1].node_begin-1;


  // Calculate fwdsel values
  j = 0;
  i = 0;
  //
  while (j < i_max)
  {
    // If current task is BWD
    if (task_data_arr[j].FSMsel == 0)
    {
      // And following task is not BWD. If unassigned the FSMsel = -1
      if (task_data_arr[j+1].FSMsel == -1)
      {
        for (i=0; i<j; i++)
        {
          if (task_data_arr[i].loop_addr == task_data_arr[j].loop_addr)
            task_data_arr[j].fwdsel = task_data_arr[i].fwdsel + 1;
        }
      }
    }

    j++;
  }

  j = 0;
  //
  while (j < i_max)
  {
    // If current task not BWD
    if (task_data_arr[j].FSMsel == -1)
    {
      // Declare it as FWD
      task_data_arr[j].FSMsel = 1;
    }

    j++;
  }

  for (i=0; i<i_max; i++)
  {
    task_data_arr[i].FSMsel = task_data_arr[i+1].FSMsel;
  }

  // Find inner loops
  task_data_arr[0].inner_loop = 0;
  //
  i = 0;
  //
  while (i < i_max)
  {
    /// FWD -> BWD transition identifies an inner loop
    // If the current task is of FWD type
    if (task_data_arr[i].FSMsel == 1)
    {
      // If the following task is of BWD type
      if (task_data_arr[i+1].FSMsel == 0)
        task_data_arr[i+1].inner_loop = 1;
    }

    i++;
  }

  // Prepare task bwd0
  task_data_arr[i_max-1].loop_addr = 0;
  task_data_arr[i_max-1].FSMsel = 0;

  // DEBUG OUTPUT
    dbg_printf("\n");
    //
    for (i=0; i<i_max; i++)
    {
      dbg_printf("begin = %d  ",task_data_arr[i].node_begin);
      dbg_printf("end = %d  ",task_data_arr[i].node_end);
      dbg_printf("taskID = %d  ",task_data_arr[i].taskid);
      dbg_printf("FSMsel = %d  ",task_data_arr[i].FSMsel);
      dbg_printf("fwdsel = %d  ",task_data_arr[i].fwdsel);
      dbg_printf("loop_addr = %d  ",task_data_arr[i].loop_addr);
      dbg_printf("inner_loop = %d\n",task_data_arr[i].inner_loop);
      //
      dbg_printf("%d: BB list (%d) = ", i, task_data_arr[i].bb_list_size);
      for (j=0; j<task_data_arr[i].bb_list_size; j++)
        dbg_printf("%d ",task_data_arr[i].bb_list[j]);
      //
      dbg_printf("\n");
    }

  // If selected option is meaningful, the DPTG is generated
  //if (gen_lut_file == 1 || gen_fsm_file == 1 || gen_vcg_file == 1 || gen_cac_file == 1)
  //{
    // Generate initial graph
    generate_graph();
/*
    // DEBUG OUTPUT
    dbg_printf("\nREPORTING EDGE LIST PRIOR ANY TCFG MANIPULATION\n");
    //
    for (m=0; m<edge_list_max; m++)
    {
      dbg_printf("Reporting edge %d: ", m);
      dbg_printf("TAIL = ");
      print_data_task(stdout,edge_list[m][TAIL]);
      dbg_printf("  HEAD = ");
      print_data_task(stdout,edge_list[m][HEAD]);
      dbg_printf("  WEIGHT = ");
      print_weight(stdout,edge_list[m][WEIGHT]);
      dbg_printf("\n");
    }
  //}
*/
  // Find the maximum value of fwdsel field for the algorithm
  // This value determines the fwdsel field bitwidth
  fwdsel_max = 0;
  // Parse all nodes in the data processing task graph (DPTG)
  for (i=0; i<i_max; i++)
  {
    if (task_data_arr[i].fwdsel > fwdsel_max)
    {
      fwdsel_max = task_data_arr[i].fwdsel;
    }
  }

  // DEBUG OUTPUT
  dbg_printf("\nMaximum fwdsel value: fwdsel_max = %d\n", fwdsel_max);
  dbg_printf("fwdsel_max bitwidth = %d\n", log2(fwdsel_max+1));

  // Find the maximum value of loop_addr field for the algorithm
  // This value determines the loop_addr field bitwidth
  nlp = 0;
  // Parse all nodes in the data processing task graph (DPTG)
  for (i=0; i<i_max; i++)
  {
    if (task_data_arr[i].loop_addr > nlp)
    {
      nlp = task_data_arr[i].loop_addr;
    }
  }

  // DEBUG OUTPUT
  dbg_printf("Maximum loop_addr value: nlp = %d\n", nlp);
  dbg_printf("loop_addr bitwidth = %d\n", log2(nlp+1));

  generate_tcfg_entries(task_data_arr);

  if (gen_lut_file_g)
  {
    strcpy(lut_file_name,copied_cur_proc_name);
    strcat(lut_file_name,".lut");
    file_lut = fopen(lut_file_name,"w");
    write_file_lut(file_lut);
  }

  if (gen_vcg_file_g)
  {
    strcpy(vcg_file_name,copied_cur_proc_name);
    strcat(vcg_file_name,".vcg");
    file_vcg = fopen(vcg_file_name,"w");
    write_file_vcg(file_vcg);
  }

  if (gen_fsm_file_g)
  {
    strcpy(fsm_file_name,copied_cur_proc_name);
    strcat(fsm_file_name,".fsm");
    file_fsm = fopen(fsm_file_name,"w");
    write_file_fsm(file_fsm);
  }

  if (gen_cac_file_g)
  {
    strcpy(cac_file_name,copied_cur_proc_name);
    strcat(cac_file_name,".cac");
    file_cac = fopen(cac_file_name,"w");
    write_file_cac(file_cac);
  }
}

int get_bb_task_num(task_data task_data_arr_in[100], unsigned int bb_num, unsigned int num_tasks)
{
  unsigned int i,j;
  int bb_task_num;

  for (i=0; i<num_tasks; i++)
  {
    for (j=0; j<task_data_arr_in[i].bb_list_size; j++)
    {
      if ((unsigned)task_data_arr_in[i].bb_list[j] == bb_num)
      {
        bb_task_num = i;
	return (bb_task_num);
      }
    }
  }

  return (-1);
}

int get_loop_initialization_bb_num(task_data task_data_arr_in[100], unsigned int loop_num, unsigned int num_tasks)
{
  unsigned int i,j;
  int fwd_for_innerbwd;
  int loop_init_bb_num;
  int bwd_task_id;

  for (i=0; i<num_tasks; i++)
  {
    if (task_data_arr[i].FSMsel == 0 &&
        task_data_arr[i].loop_addr == loop_num)
    {
      bwd_task_id = i;
    }
  }

  // it is an inner-loop bwd task
  if (task_data_arr_in[bwd_task_id].inner_loop == 1)
  {
    // the requested task is the adjacent predecessing task to the bwd task
    fwd_for_innerbwd = bwd_task_id-1;
    loop_init_bb_num = task_data_arr_in[fwd_for_innerbwd].bb_list[task_data_arr_in[fwd_for_innerbwd].bb_list_size-1];
    return (loop_init_bb_num);
  }
  else if (task_data_arr_in[bwd_task_id].inner_loop == 0)
  {
    // we are looking for a fwd task with subaddress ZERO, and loop_addr equals to loop_num
    for (j=0; j<num_tasks; j++)
    {
      if (task_data_arr_in[j].FSMsel == 1 &&
          task_data_arr_in[j].fwdsel == 0 &&
	  task_data_arr_in[j].loop_addr == loop_num)
      {
	// Just a (naive) hack
        // access the last BB in this task
        loop_init_bb_num = task_data_arr_in[j-1].bb_list[task_data_arr_in[j-1].bb_list_size-1];
        return (loop_init_bb_num);
      }
    }
  }

  return (-1);
}

int get_max_loop_num(task_data task_data_arr_in[100], unsigned int num_tasks)
{
  unsigned int i;
  int max_loop_num=0;

  for (i=0; i<num_tasks; i++)
  {
    if (task_data_arr_in[i].loop_addr > (unsigned)max_loop_num)
    {
      max_loop_num = task_data_arr_in[i].loop_addr;
    }
  }

  return (max_loop_num);
}

int get_max_task_id(task_data task_data_arr_in[100], unsigned int num_tasks)
{
  unsigned int i;
  int max_task_id=0;

  for (i=0; i<num_tasks; i++)
  {
    if (task_data_arr_in[i].taskid > (unsigned)max_task_id)
    {
      max_task_id = task_data_arr_in[i].taskid;
    }
  }

  return (max_task_id);
}

unsigned bool2uint(bool bool_val)
{
  unsigned uint_val;

  if (bool_val == true)
    uint_val = 1;
  else //if (c == 'N')
    uint_val = 0;

  return uint_val;
}

unsigned translate_char(char c)
{
  unsigned c_equiv;

  if (c == 'Y')
    c_equiv = 1;
  else if (c == 'N')
    c_equiv = 0;

  return c_equiv;
}

unsigned get_item(LNODE *ptr)
{
  unsigned tos_item;

  if (ptr != NULL)
   tos_item = ptr->data;

  return tos_item;
}

void show_item(LNODE *ptr)
{
	if (ptr != NULL)
	 printf("%d", ptr->data);
}

void show_stack(LNODE *ptr)
{
	while (ptr != NULL)
	{
		printf("%d", ptr->data);
		ptr = ptr->link;
	}
	printf("\n");
}

unsigned empty(LNODE *ptr)
{
	if (ptr == NULL)
		return(TRUE);
	else
		return(FALSE);
}

void push(LNODE **ptr, unsigned item)
{
     LNODE *p;

     p = (LNODE*)malloc(sizeof(LNODE));

     if (p == NULL)
	       printf("Error! Can not push new item.\n");
     else
     {
	  p->data = item;

	  if (empty(*ptr))
	       p->link = NULL;
	  else
	       p->link = *ptr;

	  *ptr = p;
     }
}

void pop(LNODE **ptr, unsigned *item)
{
	LNODE *p1;

	p1 = *ptr;
	if (empty(p1))
	{
		printf("Error! The stack is empty.\n");
		*item = '\0';
	}
	else
	{
		*item = p1->data;
		*ptr = p1->link;
		free(p1);
	}
}

unsigned dectobin(unsigned bin_data, int num_bits)
{
   int count;
   unsigned MASK;
   unsigned result;
   unsigned result_arr[100];

   count = num_bits;
   MASK = 1<<(count-1);

   result = 0;

   for (count=num_bits-1; count>-1; count--)
   {
     result_arr[count] = (( bin_data & MASK ) ? 1 : 0 );
     bin_data <<= 1;
   }

   for (count=num_bits-1; count>-1; count--)
     result = ipow(10,count)*result_arr[count] + result;

   return result;

}

unsigned ipow(unsigned x, unsigned y)
{
  unsigned i;
  unsigned result;

  result = 1;

  for (i=1; i<=y; i++)
    result = result*x;

  return result;

}

/* log2 function for integers: unsigned log2(unsigned operand) */
unsigned log2(unsigned operand)
{
	unsigned temp;
	unsigned log_val;

	temp = operand-1;
	log_val = 0;

	while (temp > 0)
	{
		temp = temp/2;
		log_val = log_val + 1;
	}

	return log_val;
}


void print_rom_data(FILE *outfile, int i)
{

  // Print FSMsel value
  fprintf(outfile,"%d",task_data_arr[i].FSMsel);

  // Print fwdsel value
  switch (log2(fwdsel_max+1))
  {
    case 1:
      fprintf(outfile,"%d",dectobin(task_data_arr[i].fwdsel,log2(fwdsel_max+1)));
      break;
    case 2:
      fprintf(outfile,"%02d",dectobin(task_data_arr[i].fwdsel,log2(fwdsel_max+1)));
      break;
    case 3:
      fprintf(outfile,"%03d",dectobin(task_data_arr[i].fwdsel,log2(fwdsel_max+1)));
      break;
    case 4:
      fprintf(outfile,"%04d",dectobin(task_data_arr[i].fwdsel,log2(fwdsel_max+1)));
      break;
    case 5:
      fprintf(outfile,"%05d",dectobin(task_data_arr[i].fwdsel,log2(fwdsel_max+1)));
      break;
    default:
      break;
  }

  // Print loop_addr value
  switch (log2(nlp+1))
  {
    case 0:
    case 1:
      fprintf(outfile,"%d",dectobin(task_data_arr[i].loop_addr,log2(nlp+1)));
      break;
    case 2:
      fprintf(outfile,"%02d",dectobin(task_data_arr[i].loop_addr,log2(nlp+1)));
      break;
    case 3:
      fprintf(outfile,"%03d",dectobin(task_data_arr[i].loop_addr,log2(nlp+1)));
      break;
    case 4:
      fprintf(outfile,"%04d",dectobin(task_data_arr[i].loop_addr,log2(nlp+1)));
      break;
    case 5:
      fprintf(outfile,"%05d",dectobin(task_data_arr[i].loop_addr,log2(nlp+1)));
      break;
    default:
      break;
  }

}

/* itob: decimal to binary string convrersion */
void itob(unsigned i, char *s, int num_bits)
{
  int j;

  for (j=num_bits-1; j>=0; j--, s++)
    *s = ((char) (((1UL<<j)&i)!=0)?'1':'0');

  *s='\0';
}


void write_file_lut(
                      FILE *outfile     // Name for the output file -- e.g. loop_rom.vhd
                     )
{
  unsigned i;

  // Get current time
  time(&t);

  /* Generate interface for the VHDL file */
  /* Comments */
  fprintf(outfile,"-- VHDL source for the loop_count_unit LUT generated by \"lcugen\"\n");
  fprintf(outfile,"-- Filename: %s\n", lut_file_name);
  fprintf(outfile,"-- Author: Nick Kavvadias, <nkavv@skiathos.physics.auth.gr>\n");
  fprintf(outfile,"-- Date: %s", ctime(&t));
  fprintf(outfile,"--\n");
  fprintf(outfile,"\n");

  /* Code generation for library inclusions */
  fprintf(outfile,"library IEEE;\n");
  fprintf(outfile,"use IEEE.std_logic_1164.all;\n");
  fprintf(outfile,"use IEEE.std_logic_unsigned.all;\n");
  fprintf(outfile,"use WORK.useful_functions_pkg.all;\n");
  fprintf(outfile,"\n");

  /* Generate entity declaration */
  fprintf(outfile,"entity lcu_lut is\n");
  fprintf(outfile,"\tgeneric (\n");
  //
  if (fwdsel_max > 0)
    fprintf(outfile,"\t\tFWDSEL_MAX : integer := %d;\n", fwdsel_max);
  //
  fprintf(outfile,"\t\tNLP : integer := %d\n", nlp);
  fprintf(outfile,"\t);\n");
  fprintf(outfile,"\tport (\n");
  fprintf(outfile,"\t\toe        : in std_logic;\n");
  fprintf(outfile,"\t\tgloop_end : in std_logic;\n");
  fprintf(outfile,"\t\tFSMsel    : in std_logic;\n");
  //
  if (fwdsel_max > 0)
    fprintf(outfile,"\t\tfwdsel    : in std_logic_vector(log2(FWDSEL_MAX+1)-1 downto 0);\n");
  //
  fprintf(outfile,"\t\tloop_addr : in std_logic_vector(log2(NLP+1)-1 downto 0);\n");
  //
  if (fwdsel_max > 0)
    fprintf(outfile,"\t\trom_data  : out std_logic_vector(log2(NLP+1)+log2(FWDSEL_MAX+1) downto 0)\n");
  else
    fprintf(outfile,"\t\trom_data  : out std_logic_vector(log2(NLP+1) downto 0)\n");
  //
  fprintf(outfile,"\t);\n");
  fprintf(outfile,"end lcu_lut;\n");
  fprintf(outfile,"\n");

  /* Generate architecture declaration */
  fprintf(outfile,"architecture synth of lcu_lut is\n");
  //
  if (fwdsel_max > 0)
    fprintf(outfile,"signal rom_addr: std_logic_vector(log2(NLP+1)+log2(FWDSEL_MAX+1)+1 downto 0);\n");
  else
    fprintf(outfile,"signal rom_addr: std_logic_vector(log2(NLP+1)+1 downto 0);\n");

  /* Continue with the rest of the architecture declaration */
  fprintf(outfile,"begin\n");
  fprintf(outfile,"\tprocess(oe, gloop_end, FSMsel, fwdsel, loop_addr)\n");
  fprintf(outfile,"\tbegin\n");
  fprintf(outfile,"\t--\n");
  fprintf(outfile,"\trom_addr <= gloop_end & FSMsel & fwdsel & loop_addr;\n");
  fprintf(outfile,"\t--\n");
  fprintf(outfile,"\tif (oe = '1') then\n");
  fprintf(outfile,"\t\tcase rom_addr is\n");

  // Iterate through all edges
  for (i=0; i<edge_list_max; i++)
  {
    // FWD -> FWD, FWD -> BWD
    if (task_data_arr[ edge_list[i][TAIL] ].FSMsel == 1)
    {

      // Entry for gloop_end = 0
      fprintf(outfile,"\t\t  when \"");
      fprintf(outfile,"0");
      print_rom_data(outfile, edge_list[i][TAIL]);
      fprintf(outfile,"\" => \"");
      print_rom_data(outfile, edge_list[i][HEAD]);
      fprintf(outfile,"\";\n");
      //
      // Entry for gloop_end = 1
      fprintf(outfile,"\t\t  when \"");
      fprintf(outfile,"1");
      print_rom_data(outfile, edge_list[i][TAIL]);
      fprintf(outfile,"\" => \"");
      print_rom_data(outfile, edge_list[i][HEAD]);
      fprintf(outfile,"\";\n");
    }

    // BWD -> BWD, BWD -> FWD
    if (task_data_arr[ edge_list[i][TAIL] ].FSMsel == 0)
    {

      // If it is entry for gloop_end = 0
      if (edge_list[i][WEIGHT] == 0)
      {
        fprintf(outfile,"\t\t  when \"");
        fprintf(outfile,"0");
        print_rom_data(outfile, edge_list[i][TAIL]);
        fprintf(outfile,"\" => \"");
        print_rom_data(outfile, edge_list[i][HEAD]);
        fprintf(outfile,"\";\n");
      }
      // else if it is entry for gloop_end = 1
      else if (edge_list[i][WEIGHT] == 1)
      {
        fprintf(outfile,"\t\t  when \"");
        fprintf(outfile,"1");
        print_rom_data(outfile, edge_list[i][TAIL]);
        fprintf(outfile,"\" => \"");
        print_rom_data(outfile, edge_list[i][HEAD]);
        fprintf(outfile,"\";\n");
      }
    }
  }
  /***************/

  fprintf(outfile,"\t\t--\n");
  fprintf(outfile,"\t\twhen others => rom_data <= (others => '0');\n");
  fprintf(outfile,"\tend case;\n");
  fprintf(outfile,"\t\t--\n");
  fprintf(outfile,"\telse\n");
  fprintf(outfile,"\t\trom_data <= (others => 'Z');\n");
  fprintf(outfile,"\tend if;\n");
  fprintf(outfile,"\tend process;\n\n");
  //
  fprintf(outfile,"end synth;\n");

}

void write_file_vcg(
                      FILE *outfile     // Name for the output file -- e.g. loop_rom.vhd
                     )
{
  unsigned i;

  /* Generate interface for the VCG file */
  /* Comments */
  /* Print initialization comments to console */
  // No comments

  /* Header and macroinstructions to the VCG interpreter */
  fprintf(outfile,"graph: { title: \"%s\"\n", vcg_file_name);
  fprintf(outfile,"\n");
  fprintf(outfile,"x: 30\n");
  fprintf(outfile,"y: 30\n");
  fprintf(outfile,"height: 800\n");
  fprintf(outfile,"width: 500\n");
  fprintf(outfile,"stretch: 60\n");
  fprintf(outfile,"shrink: 100\n");
  // For acyclic graphs only
  fprintf(outfile,"layoutalgorithm: minbackward\n");
  fprintf(outfile,"display_edge_labels: yes\n");
  fprintf(outfile,"late_edge_labels: yes\n");
  fprintf(outfile,"near_edges: yes\n");
  fprintf(outfile,"port_sharing: yes\n");
  // Additional options
  //
  fprintf(outfile,"node.borderwidth: 3\n");
  fprintf(outfile,"node.color: white\n");
  fprintf(outfile,"node.textcolor: black\n");
  fprintf(outfile,"node.bordercolor: black\n");
  fprintf(outfile,"node.shape: circle\n");
  fprintf(outfile,"edge.color: black\n");

  // Printing nodes
  for (i=0; i<i_max; i++)
  {
    fprintf(outfile,"node: { title:\"");
    // Print data task for title field
    print_data_task(outfile, i);
    fprintf(outfile,"\" label:\"");
    // Print data task for label field
    print_data_task(outfile, i);
    fprintf(outfile,"\" }\n");
  }

  fprintf(outfile,"\n");

  // Printing edges
  i = 0;
  //
  // Iterate through all edges
  for (i=0; i<edge_list_max; i++)
  {
    // CFG edge: task_data_arr[i] --> task_data_arr[i+1]
    fprintf(outfile,"edge: {sourcename:\"");
    // Print source data task for sourcename field
    print_data_task(outfile, edge_list[i][TAIL]);
    fprintf(outfile,"\" targetname:\"");
    // Print target data task for targetname field
    print_data_task(outfile, edge_list[i][HEAD]);
    //
    if (edge_list[i][WEIGHT] == -1)
      fprintf(outfile,"\" }\n");
    else if (edge_list[i][WEIGHT] == 0)
      fprintf(outfile,"\" label:\"not(gloop_end)\" }\n");
    else if (edge_list[i][WEIGHT] == 1)
      fprintf(outfile,"\" label:\"gloop_end\" }\n");
  }

  // Finalize VCG file
  fprintf(outfile,"}\n");
}

void sprint_data_task(char *outstr, int i)
{
  if (task_data_arr[i].FSMsel == 0)
    sprintf(outstr,"bwd%d",task_data_arr[i].loop_addr);
  else
    sprintf(outstr,"fwd%d(%d)",task_data_arr[i].loop_addr,task_data_arr[i].fwdsel);
}

void print_data_task(FILE *outfile, int i)
{
    // Print task type
    if (task_data_arr[i].FSMsel == 0)
      fprintf(outfile,"bwd");
    else
      fprintf(outfile,"fwd");
    // Print loop address
    fprintf(outfile,"%d",task_data_arr[i].loop_addr);
    // If fwd task print the value for the fwdsel field
    if (task_data_arr[i].FSMsel == 1)
      fprintf(outfile,"(%d)",task_data_arr[i].fwdsel);
}

void print_data_task_fsm(FILE *outfile, int i)
{
    // Print task type
    if (task_data_arr[i].FSMsel == 0)
      fprintf(outfile,"bwd");
    else
      fprintf(outfile,"fwd");
    // Print loop address
    fprintf(outfile,"%d",task_data_arr[i].loop_addr);
    // If fwd task print the value for the fwdsel field
    if (task_data_arr[i].FSMsel == 1)
      fprintf(outfile,"_%d",task_data_arr[i].fwdsel);
}

void write_file_fsm(
                      FILE *outfile     // Name for the output file -- e.g. fsm_loop.vhd
                   )
{
  unsigned i;

  // Get current time
  time(&t);

  /* Generate interface for the VHDL file */
  /* Comments */
  fprintf(outfile,"-- VHDL source for the loop_count_unit FSM generated by \"lcugen\"\n");
  fprintf(outfile,"-- Filename: %s\n", fsm_file_name);
  fprintf(outfile,"-- Author: Nick Kavvadias, <nkavv@skiathos.physics.auth.gr>\n");
  fprintf(outfile,"-- Date: %s", ctime(&t));
  fprintf(outfile,"--\n");
  fprintf(outfile,"\n");

  /* Code generation for library inclusions */
  fprintf(outfile,"library IEEE;\n");
  fprintf(outfile,"use IEEE.std_logic_1164.all;\n");
  fprintf(outfile,"use WORK.useful_functions_pkg.all;\n");
  fprintf(outfile,"\n");

  /* Generate entity declaration */
  fprintf(outfile,"entity lcu_fsm is\n");
  fprintf(outfile,"\tgeneric (\n");
  //
  if (fwdsel_max > 0)
    fprintf(outfile,"\t\tFWDSEL_MAX : integer := %d;\n", fwdsel_max);
  //
  fprintf(outfile,"\t\tNLP : integer := %d\n", nlp);
  fprintf(outfile,"\t);\n");
  fprintf(outfile,"\tport (\n");
  fprintf(outfile,"\t\tclk       : in std_logic;\n");
  fprintf(outfile,"\t\tstart     : in std_logic;\n");
  fprintf(outfile,"\t\treset     : in std_logic;\n");
  fprintf(outfile,"\t\tFSMbwd    : in std_logic;\n");
  fprintf(outfile,"\t\tFSMfwd    : in std_logic;\n");
  fprintf(outfile,"\t\tloop_end  : in std_logic;\n");
  fprintf(outfile,"\t\tFSMsel    : out std_logic;\n");
  //
  if (fwdsel_max > 0)
    fprintf(outfile,"\t\tfwdsel    : out std_logic_vector(log2(FWDSEL_MAX+1)-1 downto 0);\n");
  //
  fprintf(outfile,"\t\tloop_addr : out std_logic_vector(log2(NLP+1)-1 downto 0)\n");
  fprintf(outfile,"\t);\n");
  fprintf(outfile,"end lcu_fsm;\n");
  fprintf(outfile,"\n");

  /* Generate architecture declaration */
  fprintf(outfile,"architecture synth of lcu_fsm is\n");
  fprintf(outfile,"-- Data processing task declarations\n");

  // Print states
  for (i=0; i<i_max; i++)
  {
    fprintf(outfile,"constant ");
    // Print data task for label field
    print_data_task_fsm(outfile, i);
    //
    if (fwdsel_max > 0)
      fprintf(outfile,"\t: std_logic_vector(log2(NLP+1)+log2(FWDSEL_MAX+1) downto 0) := \"");
    else
      fprintf(outfile,"\t: std_logic_vector(log2(NLP+1) downto 0) := \"");
    //
    // Print data task encoding
    print_rom_data(outfile,i);
    fprintf(outfile,"\";\n");
  }

  fprintf(outfile,"\n");
  //
  if (fwdsel_max > 0)
    fprintf(outfile,"signal current,following: std_logic_vector(log2(NLP+1)+log2(FWDSEL_MAX+1) downto 0);\n");
  else
    fprintf(outfile,"signal current,following: std_logic_vector(log2(NLP+1) downto 0);\n");
  //
  fprintf(outfile,"--\n");
  /* Continue with the rest of the architecture declaration */
  fprintf(outfile,"begin\n");
  fprintf(outfile,"\t-- next state logic\n");
  fprintf(outfile,"\tprocess(current, start, FSMbwd, FSMfwd, loop_end)\n");
  fprintf(outfile,"\tbegin\n");
  fprintf(outfile,"\t\tcase current is\n");

  // Iterate through all edges
  for (i=0; i<edge_list_max; i++)
  {
    fprintf(outfile,"\t\t  when ");
    print_data_task_fsm(outfile, edge_list[i][TAIL]);
    fprintf(outfile," =>\n");

    // FWD -> FWD, FWD -> BWD
    if (task_data_arr[ edge_list[i][TAIL] ].FSMsel == 1)
    {
      // If it is the first task of the algorithm (FWD0_0)
      if (task_data_arr[ edge_list[i][TAIL] ].loop_addr == 0)
      {
        fprintf(outfile,"\t\t\tif (start = '1') then\n");
      }
      else
      {
        fprintf(outfile,"\t\t\tif (FSMfwd = '1') then\n");
      }
      //
      fprintf(outfile,"\t\t\t  following <= ");
      print_data_task_fsm(outfile,edge_list[i][HEAD]);
      fprintf(outfile,";\n");                            
      fprintf(outfile,"\t\t\telse\n");
      fprintf(outfile,"\t\t\t  following <= ");
      print_data_task_fsm(outfile,edge_list[i][TAIL]);
      fprintf(outfile,";\n");                            
      fprintf(outfile,"\t\t\tend if;\n");
    }

    // BWD -> BWD, BWD -> FWD
    if (task_data_arr[ edge_list[i][TAIL] ].FSMsel == 0)
    {
      fprintf(outfile,"\t\t\tif (FSMbwd = '1' and loop_end = '1') then\n");
      fprintf(outfile,"\t\t\t  following <= ");
      //
      i++;
      print_data_task_fsm(outfile,edge_list[i][HEAD]);
      fprintf(outfile,";\n");
      //
      i--;
      //
      // if not an inner loop task
      if (task_data_arr[ edge_list[i][TAIL] ].inner_loop == 0)
      {
        fprintf(outfile,"\t\t\telsif (FSMbwd = '1' and loop_end = '0') then\n");
        fprintf(outfile,"\t\t\t  following <= ");
        print_data_task_fsm(outfile,edge_list[i][HEAD]);
        fprintf(outfile,";\n");                            
        //
        fprintf(outfile,"\t\t\telse\n");
        fprintf(outfile,"\t\t\t  following <= ");
        print_data_task_fsm(outfile,edge_list[i][TAIL]);
        fprintf(outfile,";\n");
      }
      else
      {                                              
        i++;
        fprintf(outfile,"\t\t\telse\n");
        fprintf(outfile,"\t\t\t  following <= ");
        print_data_task_fsm(outfile,edge_list[i][TAIL]);         
        fprintf(outfile,";\n");
      } 
      //
      fprintf(outfile,"\t\t\tend if;\n");                     
      //
      if (task_data_arr[ edge_list[i][TAIL] ].inner_loop == 0)
        i++;
    }

  }

  fprintf(outfile,"\t\t  when bwd0 =>\n");                                      
  fprintf(outfile,"\t\t\tif (FSMbwd = '1') then\n");
  fprintf(outfile,"\t\t\t  following <= fwd0_0;\n");
  fprintf(outfile,"\t\t\telse\n");
  fprintf(outfile,"\t\t\t  following <= bwd0;\n");
  fprintf(outfile,"\t\t\tend if;\n");
  //                                              
  fprintf(outfile,"\t\t  -- all other cases\n");
  fprintf(outfile,"\t\t  when others =>\n");
  fprintf(outfile,"\t\t\t  following <= fwd0_0;\n");
  fprintf(outfile,"\t\t\t--\n");

  fprintf(outfile,"\t\tend case;\n");
  fprintf(outfile,"\tend process;\n");
  fprintf(outfile,"\n");

  fprintf(outfile,"\t-- current state logic\n");
  fprintf(outfile,"\tprocess(clk, reset)\n");
  fprintf(outfile,"\tbegin\n");
  fprintf(outfile,"\t\tif (reset='1') then\n");
  fprintf(outfile,"\t\t  current <= fwd0_0;\n");
  fprintf(outfile,"\t\telsif (clk'event and clk='1') then\n");
  fprintf(outfile,"\t\t  current <= following;\n");
  fprintf(outfile,"\t\tend if;\n");
  fprintf(outfile,"\tend process;\n");
  fprintf(outfile,"\n");

  fprintf(outfile,"\t-- output logic\n");
  fprintf(outfile,"\tprocess(current)\n");
  fprintf(outfile,"\tbegin\n");
  fprintf(outfile,"\t\t-- In all cases, the output signals are fields of the current state\n");
  //
  if (fwdsel_max > 0)
  {
    fprintf(outfile,"\t\tFSMsel <= current(log2(NLP+1)+log2(FWDSEL_MAX+1));\n");
    fprintf(outfile,"\t\tfwdsel <= current(log2(NLP+1)+log2(FWDSEL_MAX+1)-1 downto log2(NLP+1));\n");
  }
  else
    fprintf(outfile,"\t\tFSMsel <= current(log2(NLP+1));\n");
  //
  fprintf(outfile,"\t\tloop_addr <= current(log2(NLP+1)-1 downto 0);\n");
  fprintf(outfile,"\tend process;\n");
  fprintf(outfile,"\n");

  fprintf(outfile,"end synth;\n");

}

void write_file_cac(FILE *outfile)
{
  unsigned i;
  unsigned int cac_task_id=0;

  // Printing edges
  i = 0;
  //
  // Iterate through all edges
  for (i=0; i<edge_list_max; i++)
  {
    // CFG edge: task_data_arr[i] --> task_data_arr[i+1]
    fprintf(outfile,"//# ");
    // Print source data task for sourcename field
    print_data_task(outfile, edge_list[i][TAIL]);
    fprintf(outfile," -> ");
    // Print target data task for targetname field
    print_data_task(outfile, edge_list[i][HEAD]);
    fprintf(outfile,"\n");

    if (edge_list[i][WEIGHT] == -1)
    {
      fprintf(outfile,"ttlut_mem[0x%x].task_data=0x%x; ",cac_task_id, task_data_arr[edge_list[i][HEAD]].taskid);
      //
      if (task_data_arr[ edge_list[i][HEAD] ].FSMsel == 1)
        fprintf(outfile,"ttlut_mem[0x%x].ttsel=0x1; ",cac_task_id);
      else if (task_data_arr[ edge_list[i][HEAD] ].FSMsel == 0)
        fprintf(outfile,"ttlut_mem[0x%x].ttsel=0x0; ",cac_task_id);
      //
      fprintf(outfile,"ttlut_mem[0x%x].loop_addr=0x%x;\n",cac_task_id,task_data_arr[ edge_list[i][HEAD] ].loop_addr);
      cac_task_id++;

      fprintf(outfile,"ttlut_mem[0x%x].task_data=0x%x; ",cac_task_id, task_data_arr[edge_list[i][HEAD]].taskid);
      //
      if (task_data_arr[ edge_list[i][HEAD] ].FSMsel == 1)
        fprintf(outfile,"ttlut_mem[0x%x].ttsel=0x1; ",cac_task_id);
      else if (task_data_arr[ edge_list[i][HEAD] ].FSMsel == 0)
        fprintf(outfile,"ttlut_mem[0x%x].ttsel=0x0; ",cac_task_id);
      //
      fprintf(outfile,"ttlut_mem[0x%x].loop_addr=0x%x;\n",cac_task_id,task_data_arr[ edge_list[i][HEAD] ].loop_addr);
      cac_task_id++;
    }
    else if (edge_list[i][WEIGHT] == 0)
    {
      fprintf(outfile,"ttlut_mem[0x%x].task_data=0x%x; ",cac_task_id, task_data_arr[edge_list[i][HEAD]].taskid);
      //
      if (task_data_arr[ edge_list[i][HEAD] ].FSMsel == 1)
        fprintf(outfile,"ttlut_mem[0x%x].ttsel=0x1; ",cac_task_id);
      else if (task_data_arr[ edge_list[i][HEAD] ].FSMsel == 0)
        fprintf(outfile,"ttlut_mem[0x%x].ttsel=0x0; ",cac_task_id);
      //
      fprintf(outfile,"ttlut_mem[0x%x].loop_addr=0x%x;\n",cac_task_id,task_data_arr[ edge_list[i][HEAD] ].loop_addr);
      cac_task_id++;
    }
    else if (edge_list[i][WEIGHT] == 1)
    {
      fprintf(outfile,"ttlut_mem[0x%x].task_data=0x%x; ",cac_task_id, task_data_arr[edge_list[i][HEAD]].taskid);
      //
      if (task_data_arr[ edge_list[i][HEAD] ].FSMsel == 1)
        fprintf(outfile,"ttlut_mem[0x%x].ttsel=0x1; ",cac_task_id);
      else if (task_data_arr[ edge_list[i][HEAD] ].FSMsel == 0)
        fprintf(outfile,"ttlut_mem[0x%x].ttsel=0x0; ",cac_task_id);
      //
      fprintf(outfile,"ttlut_mem[0x%x].loop_addr=0x%x;\n",cac_task_id,task_data_arr[ edge_list[i][HEAD] ].loop_addr);
      cac_task_id++;
    }
  }

  // Finalize VCG file
  fprintf(outfile,"\n");
}

void generate_tcfg_entries(task_data task_data_arr_in[100])
{
  unsigned i;
  unsigned int cac_task_id=0;

  // Printing edges
  i = 0;
  //
  // Iterate through all edges
  for (i=0; i<edge_list_max; i++)
  {
    // CFG edge: task_data_arr_in[i] --> task_data_arr_in[i+1]

    if (edge_list[i][WEIGHT] == -1)
    {
      TCFG[cac_task_id].current_taskid = task_data_arr_in[edge_list[i][TAIL]].taskid;
      TCFG[cac_task_id].next_taskid    = task_data_arr_in[edge_list[i][HEAD]].taskid;
      //
      if (task_data_arr_in[ edge_list[i][HEAD] ].FSMsel == 1)
        TCFG[cac_task_id].next_ttsel   = 1;
      else if (task_data_arr_in[ edge_list[i][HEAD] ].FSMsel == 0)
        TCFG[cac_task_id].next_ttsel   = 0;
      //
      TCFG[cac_task_id].next_loop_addr = task_data_arr_in[ edge_list[i][HEAD] ].loop_addr;

      cac_task_id++;

      TCFG[cac_task_id].current_taskid = task_data_arr_in[edge_list[i][TAIL]].taskid;
      TCFG[cac_task_id].next_taskid    = task_data_arr_in[edge_list[i][HEAD]].taskid;
      //
      if (task_data_arr_in[ edge_list[i][HEAD] ].FSMsel == 1)
        TCFG[cac_task_id].next_ttsel   = 1;
      else if (task_data_arr_in[ edge_list[i][HEAD] ].FSMsel == 0)
        TCFG[cac_task_id].next_ttsel   = 0;
      //
      TCFG[cac_task_id].next_loop_addr = task_data_arr_in[ edge_list[i][HEAD] ].loop_addr;

      cac_task_id++;
    }
    else if (edge_list[i][WEIGHT] == 0 || edge_list[i][WEIGHT] == 1)
    {
      TCFG[cac_task_id].current_taskid = task_data_arr_in[edge_list[i][TAIL]].taskid;
      TCFG[cac_task_id].next_taskid    = task_data_arr_in[edge_list[i][HEAD]].taskid;
      //
      if (task_data_arr_in[ edge_list[i][HEAD] ].FSMsel == 1)
        TCFG[cac_task_id].next_ttsel   = 1;
      else if (task_data_arr_in[ edge_list[i][HEAD] ].FSMsel == 0)
        TCFG[cac_task_id].next_ttsel   = 0;
      //
      TCFG[cac_task_id].next_loop_addr = task_data_arr_in[ edge_list[i][HEAD] ].loop_addr;

      cac_task_id++;
    }
  }
  
  cac_task_id_max = cac_task_id;
}

unsigned int lsr_by_1(unsigned int val)
{
  unsigned int res;

  res = val >> 1;

  return (res);
}

void remove_task_data(int i)
{
  unsigned int k;
  unsigned flag;

  flag = 0;

  // 1. Remove a specific task from the task data array
  for (k=0; k<i_max; k++)
  {
    if (k==(unsigned)i)
    {
      // delete that task
      flag = 1;
    }

    if (flag==1)
    {
      task_data_arr[k].node_begin = task_data_arr[k+1].node_begin;
      task_data_arr[k].node_end   = task_data_arr[k+1].node_end;
      task_data_arr[k].taskid     = task_data_arr[k+1].taskid - 1;
      task_data_arr[k].FSMsel     = task_data_arr[k+1].FSMsel;
      task_data_arr[k].fwdsel     = task_data_arr[k+1].fwdsel;
      task_data_arr[k].loop_addr  = task_data_arr[k+1].loop_addr;
      task_data_arr[k].inner_loop = task_data_arr[k+1].inner_loop;
      task_data_arr[k].annotation = task_data_arr[k+1].annotation;
    }
  }

  // Remove dummy task
  i_max = i_max - 1;
}

// Assign edges to following node of the node to be removed
// Inputs: j node of the DPTG
// Result: create the appropriate {i,j+1,WEIGHT} edges
void assign_edges(int j)
{
  unsigned int k;

  // 1. Assign a new edge to the following node of an "annotated" node
  for (k=0; k<edge_list_max; k++)
  {
    if (edge_list[k][HEAD] == j)
    {
      // create a new edge between edge_list[k][TAIL] and j
      // New edge is {edge_list[k][TAIL],j+1,edge_list[k][WEIGHT]}
      edge_list[k][HEAD] = j+1;

      dbg_printf("Found a common edge: {%d,%d}\n",edge_list[k][TAIL],edge_list[k][HEAD]);
    }
  }

}

// Clear the edges that involve the node to be removed
void remove_common_edges(int j)
{
  unsigned int k,l;

  k = 0;

  // 2. Remove common edges
  while (k<edge_list_max)
  {
    // If the removed node is TAIL of this edge
    // only TAILS are expected...
    if (edge_list[k][TAIL]==j)
    {
      dbg_printf("Deleting a common edge: {%d,%d}\n",edge_list[k][TAIL],edge_list[k][HEAD]);

      // delete that edge from the edge-list
      for (l=k; l<edge_list_max; l++)
      {
        edge_list[l][TAIL] = edge_list[l+1][TAIL];
        edge_list[l][HEAD] = edge_list[l+1][HEAD];
        edge_list[l][WEIGHT] = edge_list[l+1][WEIGHT];
      }

      // Remove last "dummy" edge from the edge_list
      edge_list_max = edge_list_max - 1;
    }

    // Increment k
    k++;
  }

}

// Rearrange the edge list
void rearrange_edge_list(int j)
{
  unsigned int k;

  // Rename nodes and rearrange the edge list
  for (k=0; k<edge_list_max; k++)
  {
    if (edge_list[k][TAIL]>j)
    {
      // if edge_list[k][TAIL]>j then edge_list[k][TAIL]--
      edge_list[k][TAIL] = edge_list[k][TAIL]-1;
    }

    if (edge_list[k][HEAD]>j)
    {
      // if edge_list[k][HEAD]>j then edge_list[k][HEAD]--
      edge_list[k][HEAD] = edge_list[k][HEAD]-1;
    }
  }

}
/*
void load_annot_file()
{
  int size, size_max, i;
  char FSMsel_s[4];
  char c;
  unsigned loop_addr, fwdsel;

  size = 0;
  //
  while ( (c=getc(file_annot)) != EOF )
  {
    ungetc(c,file_annot);

    fscanf(file_annot,"%3s%d(%d)", FSMsel_s, &loop_addr, &fwdsel);
    FSMsel_s[4] = '\0';

    dbg_printf("FSMsel_s = %s\n",FSMsel_s);

    task_annot_arr[size].fwdsel = fwdsel;
    task_annot_arr[size].loop_addr = loop_addr;

    if (strcmp(FSMsel_s,"bwd") == 0)
    {
      task_annot_arr[size].FSMsel = 0;
    }
    else if (strcmp(FSMsel_s,"fwd") == 0)
    {
      task_annot_arr[size].FSMsel = 1;
    }

    size++;

  }

  size_max = size-1;
  //
  dbg_printf("size_max = %d\n", size_max);

  // Search task_data_arr[] entries to mark the annotation field
  //
  //if (size_max > 0)
  //{
    size = 0;
    //
    for (i=0; i<i_max; i++)
    {
      // Find a match
      if ( (task_data_arr[i].FSMsel == task_annot_arr[size].FSMsel) &&
           (task_data_arr[i].fwdsel == task_annot_arr[size].fwdsel) &&
           (task_data_arr[i].loop_addr == task_annot_arr[size].loop_addr) &&
           // AND not the bwd0(0) task
           !( (task_data_arr[i].FSMsel == 0) &&
              (task_data_arr[i].fwdsel == 0) &&
              (task_data_arr[i].loop_addr == 0)
            )
         )
      {
        // Mark the annotation field of task_data_arr entry
        task_data_arr[i].annotation = 1;
        //
        size++;
      }
    }
  //}    // end if size != 0

  dbg_printf("OBTAINED ANNOTATIONS\n");
  for (size=0; size<size_max; size++)
  {
    dbg_printf("FSMsel = %d  \n",task_annot_arr[size].FSMsel);
    dbg_printf("fwdsel = %d  \n",task_annot_arr[size].fwdsel);
    dbg_printf("loop_addr = %d  \n",task_annot_arr[size].loop_addr);
  }

  fclose(file_annot);
}
*/

void generate_graph()
{
  unsigned i, j, k;

  /**************************************************************/
  /* Generate edge list for the graph representation of the LUT */
  /**************************************************************/

  i = 0;
  // j: Enumerates edges in the graph representation
  j = 0;
  //

  while (i < i_max-1)
  {
    // FWD -> FWD or FWD -> BWD task transition
    // If the current task is of FWD type
    if (task_data_arr[i].FSMsel == 1)
    {
      //
      edge_list[j][TAIL] = i;
      edge_list[j][HEAD] = i+1;
      edge_list[j][WEIGHT] = -1;      // unconditional transition
      //
      j++;
    }

    // BWD -> BWD, BWD -> FWD task transition
    // If the current task is of BWD type
    if (task_data_arr[i].FSMsel == 0)
    {
      // If the following task is of BWD type
      if (task_data_arr[i].FSMsel == 0 || task_data_arr[i].FSMsel == 1)  // identity
      {
        // Create first entry (gloop_end = 0)
        // If inner_loop task, stay resident in the same task
        if (task_data_arr[i].inner_loop == 1)
        {
          edge_list[j][TAIL] = i;
          edge_list[j][HEAD] = i;
          edge_list[j][WEIGHT] = 0;      // gloop_end = 0
          //
          j++;
        }
        else
        {
          // Find respective FWD task (e.g. fwd3(0) for bwd3)
          edge_list[j][TAIL] = i;
          //
          k = 0;
          //
          while (k<i_max-1)
          {
            if ((task_data_arr[k].FSMsel == FWD) &&
                (task_data_arr[k].fwdsel == 0) &&
                (task_data_arr[k].loop_addr == task_data_arr[i].loop_addr))
            {
              edge_list[j][HEAD] = k;
              edge_list[j][WEIGHT] = 0;      // gloop_end = 0
            }
              k++;
          }

          j++;
        }

        // Create second entry (gloop_end = 1)
        // The same behavior for inner_loop and non-inner_loop cases
        edge_list[j][TAIL] = i;
        edge_list[j][HEAD] = i+1;
        edge_list[j][WEIGHT] = 1;      // gloop_end = 1
        //
        j++;

      }
    }

    i++;
  }

  edge_list_max = j;
  //
  dbg_printf("Reporting edge_list_max after generating graph\n");
  dbg_printf("edge_list_max = %d\n",edge_list_max);
}

void print_weight(FILE *outfile, int i)
{
  // Print weight
  switch(i)
  {
    // unconditional
    case -1:
      fprintf(outfile,"1");
      break;
    // not(gloop_end)
    case 0:
      fprintf(outfile,"not(gloop_end)");
      break;
    // gloop_end
    case 1:
      fprintf(outfile,"gloop_end");
      break;
    // invalid input
    default:
      break;
  }
}

void init_task_data_arr()
{
  int i,j;

  for (i=0; i<100; i++)
  {
    task_data_arr[i].node_begin = 0;
    task_data_arr[i].node_end   = 0;
    task_data_arr[i].taskid     = 0;
    task_data_arr[i].FSMsel     = 0;
    task_data_arr[i].fwdsel     = 0;
    task_data_arr[i].loop_addr  = 0;
    task_data_arr[i].inner_loop = 0;
    task_data_arr[i].annotation = 0;
    //
    for (j=0; j<40; j++)
      task_data_arr[i].bb_list[j] = -1;
    //
    task_data_arr[i].bb_list_size = 0;
  }
}
