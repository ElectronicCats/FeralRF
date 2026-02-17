/*
 * Startup code for CC13x2/CC26x2 (GCC)
 * Based on TI startup sequence used in working examples.
 */

#include <stdint.h>

#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/setup.h>

#define WEAK_ALIAS(x) __attribute__((weak, alias(#x)))

void ResetISR(void);
static void NmiSRHandler(void);
static void FaultISRHandler(void);
static void IntDefaultHandler(void);
extern int main(void);

void NmiSR(void) WEAK_ALIAS(NmiSRHandler);
void FaultISR(void) WEAK_ALIAS(FaultISRHandler);
void MPUFaultIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void BusFaultIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void UsageFaultIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void SVCallIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void DebugMonIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void PendSVIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void SysTickIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void GPIOIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void I2CIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void RFCCPE1IntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void PKAIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void AONRTCIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void UART0IntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void AUXSWEvent0IntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void SSI0IntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void SSI1IntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void RFCCPE0IntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void RFCHardwareIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void RFCCmdAckIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void I2SIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void AUXSWEvent1IntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void WatchdogIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void Timer0AIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void Timer0BIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void Timer1AIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void Timer1BIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void Timer2AIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void Timer2BIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void Timer3AIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void Timer3BIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void CryptoIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void uDMAIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void uDMAErrIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void FlashIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void SWEvent0IntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void AUXCombEventIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void AONProgIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void DynProgIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void AUXCompAIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void AUXADCIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void TRNGIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void OSCIntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void AUXTimer2IntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void UART1IntHandler(void) WEAK_ALIAS(IntDefaultHandler);
void BatMonIntHandler(void) WEAK_ALIAS(IntDefaultHandler);

extern uint32_t _ldata;
extern uint32_t _data;
extern uint32_t _edata;
extern uint32_t _bss;
extern uint32_t _ebss;
extern uint32_t _estack;

__attribute__((section(".resetVecs"), used)) void (*const g_pfnVectors[])(void) = {
    (void (*)(void))((uintptr_t)&_estack),
    ResetISR,
    NmiSR,
    FaultISR,
    MPUFaultIntHandler,
    BusFaultIntHandler,
    UsageFaultIntHandler,
    0,
    0,
    0,
    0,
    SVCallIntHandler,
    DebugMonIntHandler,
    0,
    PendSVIntHandler,
    SysTickIntHandler,
    GPIOIntHandler,
    I2CIntHandler,
    RFCCPE1IntHandler,
    PKAIntHandler,
    AONRTCIntHandler,
    UART0IntHandler,
    AUXSWEvent0IntHandler,
    SSI0IntHandler,
    SSI1IntHandler,
    RFCCPE0IntHandler,
    RFCHardwareIntHandler,
    RFCCmdAckIntHandler,
    I2SIntHandler,
    AUXSWEvent1IntHandler,
    WatchdogIntHandler,
    Timer0AIntHandler,
    Timer0BIntHandler,
    Timer1AIntHandler,
    Timer1BIntHandler,
    Timer2AIntHandler,
    Timer2BIntHandler,
    Timer3AIntHandler,
    Timer3BIntHandler,
    CryptoIntHandler,
    uDMAIntHandler,
    uDMAErrIntHandler,
    FlashIntHandler,
    SWEvent0IntHandler,
    AUXCombEventIntHandler,
    AONProgIntHandler,
    DynProgIntHandler,
    AUXCompAIntHandler,
    AUXADCIntHandler,
    TRNGIntHandler,
    OSCIntHandler,
    AUXTimer2IntHandler,
    UART1IntHandler,
    BatMonIntHandler,
};

void ResetISR(void) {
    uint32_t *src;
    uint32_t *dst;
    volatile uint32_t *cpacr = (volatile uint32_t *)0xE000ED88u;

    /* Required TI trim sequence for proper device startup. */
    SetupTrimDevice();

    src = &_ldata;
    dst = &_data;
    // cppcheck-suppress comparePointers
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    dst = &_bss;
    // cppcheck-suppress comparePointers
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* Enable FPU (CP10/CP11 full access). */
    *cpacr |= (0xFu << 20);

    main();

    FaultISRHandler();
}

static void NmiSRHandler(void) {
    while (1) {
    }
}

static void FaultISRHandler(void) {
    while (1) {
    }
}

static void IntDefaultHandler(void) {
    while (1) {
    }
}
