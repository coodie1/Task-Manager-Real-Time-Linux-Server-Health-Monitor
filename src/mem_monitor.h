/*
 * mem_monitor.h — Memory Monitoring Interface
 * 
 * Reads /proc/meminfo to gather RAM and swap usage metrics.
 * OS Concepts: /proc filesystem, file I/O
 */

#ifndef MEM_MONITOR_H
#define MEM_MONITOR_H

#include "shared_data.h"

/*
 * Read memory metrics from /proc/meminfo.
 * Collects: MemTotal, MemFree, MemAvailable, Buffers, Cached,
 *           SwapTotal, SwapFree
 * Calculates used memory and usage percentages.
 *
 * Parameters:
 *   metrics - pointer to shared memory SystemMetrics struct
 */
void collect_mem_metrics(SystemMetrics *metrics);

#endif /* MEM_MONITOR_H */
