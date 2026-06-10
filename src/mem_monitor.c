/*
 * mem_monitor.c — Memory Monitoring Implementation
 * 
 * Reads /proc/meminfo to get RAM and swap usage statistics.
 * Parses key-value pairs from the file and calculates usage percentages.
 *
 * OS Concepts: /proc filesystem (Lab 10-11), File I/O (Lab 4)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mem_monitor.h"

/*
 * parse_meminfo_value — Extract a numeric value from a /proc/meminfo line
 *
 * /proc/meminfo format:
 *   MemTotal:       16384000 kB
 *   MemFree:         2048000 kB
 *   ...
 */
static long parse_meminfo_value(const char *line)
{
    const char *p = line;
    /* Skip to the first digit */
    while (*p && (*p < '0' || *p > '9')) p++;
    return atol(p);
}

/*
 * collect_mem_metrics — Main memory data collection function
 *
 * Reads /proc/meminfo line by line, matching keys we care about.
 * Used memory = MemTotal - MemAvailable (modern kernels)
 */
void collect_mem_metrics(SystemMetrics *metrics)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("Failed to open /proc/meminfo");
        metrics->mem_ready = 1;
        return;
    }

    char line[256];
    long mem_total = 0, mem_free = 0, mem_available = 0;
    long buffers = 0, cached = 0;
    long swap_total = 0, swap_free = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            mem_total = parse_meminfo_value(line);
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            mem_free = parse_meminfo_value(line);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            mem_available = parse_meminfo_value(line);
        } else if (strncmp(line, "Buffers:", 8) == 0) {
            buffers = parse_meminfo_value(line);
        } else if (strncmp(line, "Cached:", 7) == 0) {
            cached = parse_meminfo_value(line);
        } else if (strncmp(line, "SwapTotal:", 10) == 0) {
            swap_total = parse_meminfo_value(line);
        } else if (strncmp(line, "SwapFree:", 9) == 0) {
            swap_free = parse_meminfo_value(line);
        }
    }
    fclose(fp);

    /* Store raw values */
    metrics->mem_total_kb     = mem_total;
    metrics->mem_free_kb      = mem_free;
    metrics->mem_available_kb = mem_available;
    metrics->mem_buffers_kb   = buffers;
    metrics->mem_cached_kb    = cached;

    /* Calculate used memory (Total - Available gives actual used) */
    metrics->mem_used_kb = mem_total - mem_available;
    if (metrics->mem_used_kb < 0) metrics->mem_used_kb = 0;

    /* Calculate percentage */
    if (mem_total > 0) {
        metrics->mem_usage_percent = (float)metrics->mem_used_kb / mem_total * 100.0;
    } else {
        metrics->mem_usage_percent = 0.0;
    }

    /* Swap metrics */
    metrics->swap_total_kb = swap_total;
    metrics->swap_free_kb  = swap_free;
    metrics->swap_used_kb  = swap_total - swap_free;
    if (metrics->swap_used_kb < 0) metrics->swap_used_kb = 0;

    if (swap_total > 0) {
        metrics->swap_usage_percent = (float)metrics->swap_used_kb / swap_total * 100.0;
    } else {
        metrics->swap_usage_percent = 0.0;
    }

    /* Signal that memory data is ready */
    metrics->mem_ready = 1;
}
