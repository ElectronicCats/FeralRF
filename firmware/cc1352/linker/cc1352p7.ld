/* CC1352P7 Linker Script */
/* 704KB Flash, 144KB SRAM */

MEMORY
{
    FLASH (RX)  : ORIGIN = 0x00000000, LENGTH = 0x000AFFA8  /* Flash minus CCFG lock page */
    CCFG  (RX)  : ORIGIN = 0x000AFFA8, LENGTH = 0x00000058  /* CCFG fixed address */
    SRAM  (RWX) : ORIGIN = 0x20000000, LENGTH = 0x00023000  /* 140KB data/bss */
    STACK (RWX) : ORIGIN = 0x20023000, LENGTH = 0x00001000  /* 4KB stack */
}

ENTRY(ResetISR)

SECTIONS
{
    .text :
    {
        KEEP(*(.resetVecs))
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
        . = ALIGN(4);
    } > FLASH

    _etext = .;

    .data : AT(_etext)
    {
        . = ALIGN(4);
        _data = .;
        *(.data*)
        *(.ramfunc*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM

    _ldata = LOADADDR(.data);

    .bss :
    {
        . = ALIGN(4);
        _bss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > SRAM

    .stack (NOLOAD) :
    {
        . = ORIGIN(STACK);
        . = ALIGN(8);
        _stack = .;
        . = . + LENGTH(STACK);
        _estack = .;
    } > STACK

    .ccfg :
    {
        . = ALIGN(4);
        KEEP(*(.ccfg))
        . = ALIGN(4);
    } > CCFG

    ASSERT(SIZEOF(.ccfg) == 0x58, "CCFG section must be 0x58 bytes")

    _end = .;

    /DISCARD/ :
    {
        *(.ARM.attributes*)
        *(.comment*)
        *(.note*)
    }
}
