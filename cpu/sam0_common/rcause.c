#include "cpu.h"

#ifndef RSTC_RCAUSE_POR
#define RSTC_RCAUSE_POR   PM_RCAUSE_POR
#endif
#ifndef RSTC_RCAUSE_BOD12
#define RSTC_RCAUSE_BOD12 PM_RCAUSE_BOD12
#endif
#ifndef RSTC_RCAUSE_BOD33
#define RSTC_RCAUSE_BOD33 PM_RCAUSE_BOD33
#endif
#ifndef RSTC_RCAUSE_EXT
#define RSTC_RCAUSE_EXT   PM_RCAUSE_EXT
#endif
#ifndef RSTC_RCAUSE_WDT
#define RSTC_RCAUSE_WDT   PM_RCAUSE_WDT
#endif
#ifndef RSTC_RCAUSE_SYST
#define RSTC_RCAUSE_SYST  PM_RCAUSE_SYST
#endif

const char *cpu_rcause(void)
{
#ifdef REG_PM_RCAUSE
    uint32_t rcause = PM->RCAUSE.reg;
#else
    uint32_t rcause = RSTC->RCAUSE.reg;
#endif

    if (rcause & RSTC_RCAUSE_POR) {
        return "power-on";
    }
    if (rcause & RSTC_RCAUSE_BOD12) {
        return "1.2V brown-out";
    }
    if (rcause & RSTC_RCAUSE_BOD33) {
        return "3.3V brown-out";
    }
    if (rcause & RSTC_RCAUSE_EXT) {
        return "external";
    }
    if (rcause & RSTC_RCAUSE_WDT) {
        return "watchdog";
    }
    if (rcause & RSTC_RCAUSE_SYST) {
        return "system";
    }
#ifdef RSTC_RCAUSE_BACKUP
    if (rcause & RSTC_RCAUSE_BACKUP) {
        return "backup/rtc";
    }
#endif

    return "?";
}
