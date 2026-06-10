/*
 * cpu_monitor.c — CPU Monitoring Implementation
 * 
 * Reads /proc/stat to calculate overall CPU usage using the delta method.
 * Reads /proc/cpuinfo for core count.
 * Reads /proc/loadavg for system load averages.
 *
 * OS Concepts: /proc filesystem (Lab 10-11), File I/O (Lab 4)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cpu_monitor.h"

/* Structure to hold raw CPU time values from /proc/stat */
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} CpuTimes;

/*
 * read_cpu_times — Parse the first line of /proc/stat
 * 
 * /proc/stat format:
 *   cpu  user nice system idle iowait irq softirq steal guest guest_nice
 *
 * Returns 0 on success, -1 on failure.
 */
static int read_cpu_times(CpuTimes *times)
{
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("Failed to open /proc/stat");
        return -1;
    }

    char line[256];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* Parse: "cpu  user nice system idle iowait irq softirq steal" */
    int matched = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                         &times->user, &times->nice, &times->system,
                         &times->idle, &times->iowait, &times->irq,
                         &times->softirq, &times->steal);

    return (matched >= 4) ? 0 : -1;
}

/*
 * get_core_count — Count the number of CPU cores from /proc/cpuinfo
 */
static int get_core_count(void)
{
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) return 1;

    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "processor", 9) == 0) {
            count++;
        }
    }
    fclose(fp);
    return (count > 0) ? count : 1;
}

/*
 * read_load_averages — Parse /proc/loadavg
 * 
 * Format: "0.15 0.10 0.05 1/234 5678"
 */
static void read_load_averages(SystemMetrics *metrics)
{
    FILE *fp = fopen("/proc/loadavg", "r");
    if (!fp) {
        metrics->load_avg_1 = metrics->load_avg_5 = metrics->load_avg_15 = 0.0;
        return;
    }

    if (fscanf(fp, "%f %f %f", &metrics->load_avg_1, &metrics->load_avg_5, &metrics->load_avg_15) < 3) {
        metrics->load_avg_1 = metrics->load_avg_5 = metrics->load_avg_15 = 0.0;
    }
    fclose(fp);
}

/*
 * collect_cpu_metrics — Main CPU data collection function
 *
 * Uses the delta method: takes two snapshots of /proc/stat separated
 * by a short interval, then calculates the percentage of time spent
 * in each CPU state (user, system, idle, iowait).
 */
void collect_cpu_metrics(SystemMetrics *metrics)
{
    CpuTimes t1, t2;

    /* First snapshot */
    if (read_cpu_times(&t1) != 0) {
        metrics->cpu_usage_percent = 0.0;
        metrics->cpu_ready = 1;
        return;
    }

    /* Short delay for delta calculation (500ms) */
    usleep(500000);

    /* Second snapshot */
    if (read_cpu_times(&t2) != 0) {
        metrics->cpu_usage_percent = 0.0;
        metrics->cpu_ready = 1;
        return;
    }

    /* Calculate deltas */
    unsigned long long d_user    = (t2.user + t2.nice) - (t1.user + t1.nice);
    unsigned long long d_system  = (t2.system + t2.irq + t2.softirq) - (t1.system + t1.irq + t1.softirq);
    unsigned long long d_idle    = t2.idle - t1.idle;
    unsigned long long d_iowait  = t2.iowait - t1.iowait;
    unsigned long long d_steal   = t2.steal - t1.steal;
    unsigned long long d_total   = d_user + d_system + d_idle + d_iowait + d_steal;

    if (d_total == 0) d_total = 1; /* Avoid division by zero */

    /* Calculate percentages */
    metrics->cpu_user_percent   = (float)d_user   / d_total * 100.0;
    metrics->cpu_system_percent = (float)d_system  / d_total * 100.0;
    metrics->cpu_idle_percent   = (float)d_idle    / d_total * 100.0;
    metrics->cpu_iowait_percent = (float)d_iowait  / d_total * 100.0;
    metrics->cpu_usage_percent  = 100.0 - metrics->cpu_idle_percent;

    /* Get core count */
    metrics->cpu_core_count = get_core_count();

    /* Get load averages */
    read_load_averages(metrics);

    /* Signal that CPU data is ready */
    metrics->cpu_ready = 1;
}
