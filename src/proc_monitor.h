/*
 * proc_monitor.h — Process Monitoring Interface
 * 
 * Scans /proc/[pid]/ directories to gather process state information.
 * OS Concepts: /proc filesystem, process states
 */

#ifndef PROC_MONITOR_H
#define PROC_MONITOR_H

#include "shared_data.h"

/*
 * Scan running processes via /proc/[pid]/stat and /proc/[pid]/status.
 * Finds top processes by CPU usage, counts process states
 * (running, sleeping, zombie, stopped).
 *
 * Parameters:
 *   metrics - pointer to shared memory SystemMetrics struct
 */
void collect_proc_metrics(SystemMetrics *metrics);

#endif /* PROC_MONITOR_H */
