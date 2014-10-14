/* file "tcfggen/tcfggen.h" */
/*
 *     Copyright (c) 2006, 2007, 2008, 2009, 2010,
 *                   2011, 2012, 2013, 2014 Nikolaos Kavvadias
 *
 *     This software was written by Nikolaos Kavvadias, Ph.D. candidate
 *     at the Physics Department, Aristotle University of Thessaloniki,
 *     Greece (at the time).
 *
 *     Current affiliation:
 *     Dr. Nikolaos Kavvadias
 *     Independent Consultant -- Research Scientist
 *     Kornarou 12 Rd., Nea Ampliani,
 *     35100 Lamia, Greece
 *
 *     This software is provided under the terms described in
 *     the "machine/copyright.h" include file.
 */

#ifndef TCFGGEN_TCFGGEN_H
#define TCFGGEN_TCFGGEN_H

#include <machine/copyright.h>

#ifdef USE_PRAGMA_INTERFACE
#pragma interface "tcfggen/tcfggen.h"
#endif

#include <machine/machine.h>

#define DEBUG

#ifdef DEBUG
#  include <stdio.h>
#  define dbg_printf(args...) fprintf (stderr, "DBG: " args)
#  define _d_(arg) arg
#else
#  define dbg_printf(args...)
#  define _d_(arg)
#endif

class TcfgGen {
  public:
    TcfgGen() { }

    void initialize() { }
    void do_opt_unit(OptUnit*);
    void finalize() { }

    // set pass options
    void set_gen_lut_file(bool sl)      { gen_lut_file = sl; }
    void set_gen_vcg_file(bool sl)      { gen_vcg_file = sl; }
    void set_gen_fsm_file(bool sl)      { gen_fsm_file = sl; }
    void set_gen_cac_file(bool sl)      { gen_cac_file = sl; }

  protected:
    bool gen_lut_file;
    bool gen_vcg_file;
    bool gen_fsm_file;
    bool gen_cac_file;

};

#endif /* TCFGGEN_TCFGGEN_H */
