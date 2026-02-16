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
        *(.vectors)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _data = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT > FLASH

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
}
