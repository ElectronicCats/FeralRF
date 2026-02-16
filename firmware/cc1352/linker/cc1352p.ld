/* CC1352P1F3RGZT Linker Script */
/* 352KB Flash, 80KB SRAM */

MEMORY
{
    FLASH (RX)  : ORIGIN = 0x00000000, LENGTH = 0x00058000  /* 352KB */
    SRAM (RWX)  : ORIGIN = 0x20000000, LENGTH = 0x00014000  /* 80KB */
}

ENTRY(ResetISR)

SECTIONS
{
    .text :
    {
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
        . = ALIGN(8);
        _stack = .;
        . = . + 0x1000;  /* 4KB stack */
        _estack = .;
    } > SRAM

    _end = .;

    /DISCARD/ :
    {
        *(.ARM.attributes*)
        *(.comment*)
        *(.note*)
    }
}
