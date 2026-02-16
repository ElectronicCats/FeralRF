/*
 * Startup code for CC13x2/CC26x2 (GCC)
 */

#include <stdint.h>

/* Forward declarations */
void ResetISR(void);
void NMIHandler(void) __attribute__((weak, alias("DefaultHandler")));
void HardFaultHandler(void) __attribute__((weak, alias("DefaultHandler")));
void MemManageHandler(void) __attribute__((weak, alias("DefaultHandler")));
void BusFaultHandler(void) __attribute__((weak, alias("DefaultHandler")));
void UsageFaultHandler(void) __attribute__((weak, alias("DefaultHandler")));
void SVCHandler(void) __attribute__((weak, alias("DefaultHandler")));
void DebugMonHandler(void) __attribute__((weak, alias("DefaultHandler")));
void PendSVHandler(void) __attribute__((weak, alias("DefaultHandler")));
void SysTickHandler(void) __attribute__((weak, alias("DefaultHandler")));

void DefaultHandler(void)
{
    while(1);
}

/* Main entry point */
extern int main(void);

/* Stack pointer initial value - defined in linker script */
extern uint32_t _estack;

/* Vector table */
__attribute__((section(".vectors")))
const uint32_t vectors[] = {
    (uint32_t)&_estack,         /* Initial SP */
    (uint32_t)ResetISR,         /* Reset */
    (uint32_t)NMIHandler,       /* NMI */
    (uint32_t)HardFaultHandler, /* Hard Fault */
    (uint32_t)MemManageHandler, /* MPU Fault */
    (uint32_t)BusFaultHandler,  /* Bus Fault */
    (uint32_t)UsageFaultHandler,/* Usage Fault */
    0, 0, 0, 0,                 /* Reserved */
    (uint32_t)SVCHandler,       /* SVC */
    (uint32_t)DebugMonHandler,  /* Debug Monitor */
    0,                          /* Reserved */
    (uint32_t)PendSVHandler,    /* PendSV */
    (uint32_t)SysTickHandler,   /* SysTick */
};

/* Data/BSS symbols from linker script */
extern uint32_t _ldata;
extern uint32_t _data;
extern uint32_t _edata;
extern uint32_t _bss;
extern uint32_t _ebss;

void ResetISR(void)
{
    uint32_t *src, *dst;

    /* Copy .data section from flash to SRAM */
    src = &_ldata;
    dst = &_data;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero fill .bss section */
    dst = &_bss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* Call main */
    main();

    /* Loop forever if main returns */
    while(1);
}
