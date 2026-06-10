/*
 * cpu_monitor.h — CPU Monitoring Interface
 * 
 * Reads /proc/stat and /proc/cpuinfo to gather CPU usage metrics.
 * OS Concepts: /proc filesystem, file I/O
 */

#ifndef CPU_MONITOR_H
#define CPU_MONITOR_H

#include "shared_data.h"

/*
 * Read CPU metrics from /proc/stat and /proc/cpuinfo.
 * Calculates CPU usage percentage using the delta method
 * (comparing two snapshots separated by a short interval).
 * Also reads load averages from /proc/loadavg.
 *
 * Parameters:
 *   metrics - pointer to shared memory SystemMetrics struct
 * 
 * This function is designed to run in a forked child process.
 */
void collect_cpu_metrics(SystemMetrics *metrics);

#endif /* CPU_MONITOR_H */
