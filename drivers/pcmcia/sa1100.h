/*======================================================================

    Device driver for the PCMCIA control functionality of StrongARM
    SA-1100 microprocessors.

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is John G. Dorsey
    <john+@cs.cmu.edu>.  Portions created by John G. Dorsey are
    Copyright (C) 1999 John G. Dorsey.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
======================================================================*/

#if !defined(_PCMCIA_SA1100_H)
# define _PCMCIA_SA1100_H

#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include "cs_internal.h"
#include "sa1100_generic.h"

/* MECR: Expansion Memory Configuration Register
 * (SA-1100 Developers Manual, p.10-13; SA-1110 Developers Manual, p.10-24)
 *
 * MECR layout is:  
 *
 *   FAST1 BSM1<4:0> BSA1<4:0> BSIO1<4:0> FAST0 BSM0<4:0> BSA0<4:0> BSIO0<4:0>
 *
 * (This layout is actually true only for the SA-1110; the FASTn bits are
 * reserved on the SA-1100.)
 */

#define MECR_SOCKET_0_SHIFT (0)
#define MECR_SOCKET_1_SHIFT (16)

#define MECR_BS_MASK        (0x1f)
#define MECR_FAST_MODE_MASK (0x01)

#define MECR_BSIO_SHIFT (0)
#define MECR_BSA_SHIFT  (5)
#define MECR_BSM_SHIFT  (10)
#define MECR_FAST_SHIFT (15)

#define MECR_SET(mecr, sock, shift, mask, bs) \
((mecr)=((mecr)&~(((mask)<<(shift))<<\
                  ((sock)==0?MECR_SOCKET_0_SHIFT:MECR_SOCKET_1_SHIFT)))|\
        (((bs)<<(shift))<<((sock)==0?MECR_SOCKET_0_SHIFT:MECR_SOCKET_1_SHIFT)))

#define MECR_GET(mecr, sock, shift, mask) \
((((mecr)>>(((sock)==0)?MECR_SOCKET_0_SHIFT:MECR_SOCKET_1_SHIFT))>>\
 (shift))&(mask))

#define MECR_BSIO_SET(mecr, sock, bs) \
MECR_SET((mecr), (sock), MECR_BSIO_SHIFT, MECR_BS_MASK, (bs))

#define MECR_BSIO_GET(mecr, sock) \
MECR_GET((mecr), (sock), MECR_BSIO_SHIFT, MECR_BS_MASK)

#define MECR_BSA_SET(mecr, sock, bs) \
MECR_SET((mecr), (sock), MECR_BSA_SHIFT, MECR_BS_MASK, (bs))

#define MECR_BSA_GET(mecr, sock) \
MECR_GET((mecr), (sock), MECR_BSA_SHIFT, MECR_BS_MASK)

#define MECR_BSM_SET(mecr, sock, bs) \
MECR_SET((mecr), (sock), MECR_BSM_SHIFT, MECR_BS_MASK, (bs))

#define MECR_BSM_GET(mecr, sock) \
MECR_GET((mecr), (sock), MECR_BSM_SHIFT, MECR_BS_MASK)

#define MECR_FAST_SET(mecr, sock, fast) \
MECR_SET((mecr), (sock), MECR_FAST_SHIFT, MECR_FAST_MODE_MASK, (fast))

#define MECR_FAST_GET(mecr, sock) \
MECR_GET((mecr), (sock), MECR_FAST_SHIFT, MECR_FAST_MODE_MASK)


/* This function implements the BS value calculation for setting the MECR
 * using integer arithmetic:
 */
static inline unsigned int sa1100_pcmcia_mecr_bs(unsigned int pcmcia_cycle_ns,
						 unsigned int cpu_clock_khz){
  unsigned int t = ((pcmcia_cycle_ns * cpu_clock_khz) / 6) - 1000000;
  return (t / 1000000) + (((t % 1000000) == 0) ? 0 : 1);
}

/* This function returns the (approxmiate) command assertion period, in
 * nanoseconds, for a given CPU clock frequency and MECR BS value:
 */
static inline unsigned int sa1100_pcmcia_cmd_time(unsigned int cpu_clock_khz,
						  unsigned int pcmcia_mecr_bs){
  return (((10000000 * 2) / cpu_clock_khz) * (3 * (pcmcia_mecr_bs + 1))) / 10;
}


/* SA-1100 PCMCIA Memory and I/O timing
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * The SA-1110 Developer's Manual, section 10.2.5, says the following:
 *
 *  "To calculate the recommended BS_xx value for each address space:
 *   divide the command width time (the greater of twIOWR and twIORD,
 *   or the greater of twWE and twOE) by processor cycle time; divide
 *   by 2; divide again by 3 (number of BCLK's per command assertion);
 *   round up to the next whole number; and subtract 1."
 *
 * The PC Card Standard, Release 7, section 4.13.4, says that twIORD
 * has a minimum value of 165ns. Section 4.13.5 says that twIOWR has
 * a minimum value of 165ns, as well. Section 4.7.2 (describing
 * common and attribute memory write timing) says that twWE has a
 * minimum value of 150ns for a 250ns cycle time (for 5V operation;
 * see section 4.7.4), or 300ns for a 600ns cycle time (for 3.3V
 * operation, also section 4.7.4). Section 4.7.3 says that taOE
 * has a maximum value of 150ns for a 300ns cycle time (for 5V
 * operation), or 300ns for a 600ns cycle time (for 3.3V operation).
 *
 * When configuring memory maps, Card Services appears to adopt the policy
 * that a memory access time of "0" means "use the default." The default
 * PCMCIA I/O command width time is 165ns. The default PCMCIA 5V attribute
 * and memory command width time is 150ns; the PCMCIA 3.3V attribute and
 * memory command width time is 300ns.
 */
#define SA1100_PCMCIA_IO_ACCESS      (165)
#define SA1100_PCMCIA_5V_MEM_ACCESS  (150)
#define SA1100_PCMCIA_3V_MEM_ACCESS  (300)


/* The socket driver actually works nicely in interrupt-driven form,
 * so the (relatively infrequent) polling is "just to be sure."
 */
#define SA1100_PCMCIA_POLL_PERIOD    (2*HZ)


/* This structure encapsulates per-socket state which we might need to
 * use when responding to a Card Services query of some kind.
 */
struct sa1100_pcmcia_socket {
  /*
   * Core PCMCIA state
   */
  socket_state_t        cs_state;
  pccard_io_map         io_map[MAX_IO_WIN];
  pccard_mem_map        mem_map[MAX_WIN];
  void                  (*handler)(void *, unsigned int);
  void                  *handler_info;

  struct pcmcia_state   k_state;
  ioaddr_t              phys_attr, phys_mem;
  void			*virt_io;
  unsigned short        speed_io, speed_attr, speed_mem;

  /*
   * Info from low level handler
   */
  unsigned int          irq;
};


/* I/O pins replacing memory pins
 * (PCMCIA System Architecture, 2nd ed., by Don Anderson, p.75)
 *
 * These signals change meaning when going from memory-only to 
 * memory-or-I/O interface:
 */
#define iostschg bvd1
#define iospkr   bvd2


/*
 * Declaration for all machine specific init/exit functions.
 */
extern int pcmcia_adsbitsy_init(void);
extern void pcmcia_adsbitsy_exit(void);

extern int pcmcia_assabet_init(void);
extern void pcmcia_assabet_exit(void);

extern int pcmcia_badge4_init(void);
extern void pcmcia_badge4_exit(void);

extern int pcmcia_cerf_init(void);
extern void pcmcia_cerf_exit(void);

extern int pcmcia_flexanet_init(void);
extern void pcmcia_flexanet_exit(void);

extern int pcmcia_freebird_init(void);
extern void pcmcia_freebird_exit(void);

extern int pcmcia_gcplus_init(void);
extern void pcmcia_gcplus_exit(void);

extern int pcmcia_graphicsmaster_init(void);
extern void pcmcia_graphicsmaster_exit(void);

extern int pcmcia_jornada720_init(void);
extern void pcmcia_jornada720_exit(void);

extern int pcmcia_neponset_init(void);
extern void pcmcia_neponset_exit(void);

extern int pcmcia_pangolin_init(void);
extern void pcmcia_pangolin_exit(void);

extern int pcmcia_pfs168_init(void);
extern void pcmcia_pfs168_exit(void);

extern int pcmcia_shannon_init(void);
extern void pcmcia_shannon_exit(void);

extern int pcmcia_simpad_init(void);
extern void pcmcia_simpad_exit(void);

extern int pcmcia_stork_init(void);
extern void pcmcia_stork_exit(void);

extern int pcmcia_system3_init(void);
extern void pcmcia_system3_exit(void);

extern int pcmcia_trizeps_init(void);
extern void pcmcia_trizeps_exit(void);

extern int pcmcia_xp860_init(void);
extern void pcmcia_xp860_exit(void);

extern int pcmcia_yopy_init(void);
extern void pcmcia_yopy_exit(void);

#endif  /* !defined(_PCMCIA_SA1100_H) */
