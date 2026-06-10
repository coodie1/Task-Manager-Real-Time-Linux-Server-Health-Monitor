/*
 * disk_monitor.h — Disk Monitoring Interface
 * 
 * Uses statvfs() for disk space and /proc/diskstats for I/O throughput.
 * OS Concepts: /proc filesystem, system calls
 */

#ifndef DISK_MONITOR_H
#define DISK_MONITOR_H

#include "shared_data.h"

/*
 * Read disk metrics using statvfs() and /proc/diskstats.
 * Collects: total/used/free space, usage percentage, read/write speeds.
 *
 * Parameters:
 *   metrics - pointer to shared memory SystemMetrics struct
 */
void collect_disk_metrics(SystemMetrics *metrics);

#endif /* DISK_MONITOR_H */
