#include <linux/config.h>

#include <asm/cache.h>
#include <asm/ptrace.h>
#include <asm/system.h>

OUTPUT_FORMAT("elf64-ia64-little")
OUTPUT_ARCH(ia64)
ENTRY(phys_start)
SECTIONS
{
  /* Sections to be discarded */
  /DISCARD/ : {
	*(.text.exit)
	*(.data.exit)
	*(.exitcall.exit)
	*(.IA_64.unwind.text.exit)
	*(.IA_64.unwind_info.text.exit)
	}

  v = PAGE_OFFSET;	/* this symbol is here to make debugging easier... */
  phys_start = _start - PAGE_OFFSET;

  . = KERNEL_START;

  _text = .;
  _stext = .;
  .text : AT(ADDR(.text) - PAGE_OFFSET)
    {
	*(.text.ivt)
	*(.text)
    }
  .text2 : AT(ADDR(.text2) - PAGE_OFFSET)
	{ *(.text2) }
#ifdef CONFIG_SMP
  .text.lock : AT(ADDR(.text.lock) - PAGE_OFFSET)
	{ *(.text.lock) }
#endif
  _etext = .;

  /* Read-only data */

  __gp = ALIGN(16) + 0x200000;	/* gp must be 16-byte aligned for exc. table */

  /* Global data */
  _data = .;

  /* Exception table */
  . = ALIGN(16);
  __start___ex_table = .;
  __ex_table : AT(ADDR(__ex_table) - PAGE_OFFSET)
	{ *(__ex_table) }
  __stop___ex_table = .;

#if defined(CONFIG_IA64_GENERIC)
  /* Machine Vector */
  . = ALIGN(16);
  machvec_start = .;
  .machvec : AT(ADDR(.machvec) - PAGE_OFFSET)
	{ *(.machvec) }
  machvec_end = .;
#endif

  __start___ksymtab = .;	/* Kernel symbol table */
  __ksymtab : AT(ADDR(__ksymtab) - PAGE_OFFSET)
	{ *(__ksymtab) }
  __stop___ksymtab = .;

  /* Unwind info & table: */
  . = ALIGN(8);
  .IA_64.unwind_info : AT(ADDR(.IA_64.unwind_info) - PAGE_OFFSET)
	{ *(.IA_64.unwind_info*) }
  ia64_unw_start = .;
  .IA_64.unwind : AT(ADDR(.IA_64.unwind) - PAGE_OFFSET)
	{ *(.IA_64.unwind*) }
  ia64_unw_end = .;

  .rodata : AT(ADDR(.rodata) - PAGE_OFFSET)
	{ *(.rodata) *(.rodata.*) }
  .kstrtab : AT(ADDR(.kstrtab) - PAGE_OFFSET)
	{ *(.kstrtab) }
  .opd : AT(ADDR(.opd) - PAGE_OFFSET)
	{ *(.opd) }

  /* Initialization code and data: */

  . = ALIGN(PAGE_SIZE);
  __init_begin = .;
  .text.init : AT(ADDR(.text.init) - PAGE_OFFSET)
	{ *(.text.init) }

  .data.init : AT(ADDR(.data.init) - PAGE_OFFSET)
	{ *(.data.init) }
   . = ALIGN(16);
  __setup_start = .;
  .setup.init : AT(ADDR(.setup.init) - PAGE_OFFSET)
        { *(.setup.init) }
  __setup_end = .;
  __initcall_start = .;
  .initcall.init : AT(ADDR(.initcall.init) - PAGE_OFFSET)
	{
		*(.initcall1.init)
		*(.initcall2.init)
		*(.initcall3.init)
		*(.initcall4.init)
		*(.initcall5.init)
		*(.initcall6.init)
		*(.initcall7.init)
	}
  __initcall_end = .;
  . = ALIGN(PAGE_SIZE);
  __init_end = .;

  /* The initial task and kernel stack */
  .data.init_task : AT(ADDR(.data.init_task) - PAGE_OFFSET)
	{ *(.data.init_task) }

  .data.page_aligned : AT(ADDR(.data.page_aligned) - PAGE_OFFSET)
        { *(__special_page_section)
	  __start_gate_section = .;
	  *(.text.gate)
	  __stop_gate_section = .;
	}

  . = ALIGN(SMP_CACHE_BYTES);
  .data.cacheline_aligned : AT(ADDR(.data.cacheline_aligned) - PAGE_OFFSET)
        { *(.data.cacheline_aligned) }

  /* Kernel symbol names for modules: */
  .kstrtab : AT(ADDR(.kstrtab) - PAGE_OFFSET)
	{ *(.kstrtab) }

  /* Per-cpu data: */
  . = ALIGN(PAGE_SIZE);
  __phys_per_cpu_start = .;
  .data.percpu PERCPU_ADDR : AT(__phys_per_cpu_start - PAGE_OFFSET)
	{
		__per_cpu_start = .;
		*(.data.percpu)
		__per_cpu_end = .;
	}
  . = __phys_per_cpu_start + 4096;	/* ensure percpu fits into smallest page size (4KB) */

  .data : AT(ADDR(.data) - PAGE_OFFSET)
	{ *(.data) *(.gnu.linkonce.d*) CONSTRUCTORS }

  .got : AT(ADDR(.got) - PAGE_OFFSET)
	{ *(.got.plt) *(.got) }
  /* We want the small data sections together, so single-instruction offsets
     can access them all, and initialized data all before uninitialized, so
     we can shorten the on-disk segment size.  */
  .sdata : AT(ADDR(.sdata) - PAGE_OFFSET)
	{ *(.sdata) }
  _edata  =  .;
  _bss = .;
  .sbss : AT(ADDR(.sbss) - PAGE_OFFSET)
	{ *(.sbss) *(.scommon) }
  .bss : AT(ADDR(.bss) - PAGE_OFFSET)
	{ *(.bss) *(COMMON) }

  _end = .;

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* These must appear regardless of  .  */
  /* Discard them for now since Intel SoftSDV cannot handle them.
  .comment 0 : { *(.comment) }
  .note 0 : { *(.note) }
  */
  /DISCARD/ : { *(.comment) }
  /DISCARD/ : { *(.note) }
}
