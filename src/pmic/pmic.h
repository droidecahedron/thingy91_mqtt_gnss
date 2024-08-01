#ifndef _PMIC_H_
#define _PMIC_H

#define DEBUG_USE_SYSINIT false // Set to true if you want to use sys_init the way the atv2/thingy board inits do.
#define STACKSIZE 1024
#define STARTUP_THREAD_PRIORITY -1
#define PMIC_THREAD_PRIORITY 7
#define BATTERY_SAMPLE_INTERVAL_MS 10000

#endif /* _PMIC_H_ */
