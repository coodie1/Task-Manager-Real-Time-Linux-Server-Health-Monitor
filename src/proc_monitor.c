/*
 * proc_monitor.c — Process Monitoring Implementation
 * 
 * Scans /proc/[pid]/stat for all running processes to collect:
 *   - Process name, state, CPU time, memory usage, thread count
 *   - Counts of running, sleeping, zombie, and stopped processes
 *   - Top N processes sorted by CPU usage
 *
 * OS Concepts: /proc filesystem (Lab 10-11), Process states (Lab 6),
 *              Directory traversal, Sorting algorithms
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include "proc_monitor.h"

/* Maximum processes we'll scan */
#define MAX_SCAN_PROCS 1024

/* Internal structure for collecting process data */
typedef struct {
    int    pid;
    char   name[MAX_PROC_NAME];
    char   state;
    unsigned long long utime;   /* User mode CPU time (in clock ticks) */
    unsigned long long stime;   /* Kernel mode CPU time */
    long   rss_pages;           /* Resident Set Size in pages */
    int    threads;
} ProcData;

/*
 * get_total_cpu_time — Read total CPU time from /proc/stat
 * Returns total jiffies (clock ticks) across all CPUs.
 */
static unsigned long long get_total_cpu_time(void)
{
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0;

    char line[256];
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    user = nice = system = idle = iowait = irq = softirq = steal = 0;

    if (fgets(line, sizeof(line), fp)) {
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    }
    fclose(fp);

    return user + nice + system + idle + iowait + irq + softirq + steal;
}

/*
 * read_proc_stat — Parse /proc/[pid]/stat for a single process
 *
 * /proc/[pid]/stat format:
 *   pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt
 *   majflt cmajflt utime stime cutime cstime priority nice num_threads ...
 *   ... starttime vsize rss ...
 *
 * Fields 14,15 = utime,stime; Field 20 = num_threads; Field 24 = rss
 */
static int read_proc_stat(int pid, ProcData *pd)
{
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[1024];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* Find the process name between '(' and ')' — handles names with spaces */
    char *name_start = strchr(line, '(');
    char *name_end   = strrchr(line, ')');
    if (!name_start || !name_end || name_end <= name_start) return -1;

    /* Extract name */
    int name_len = name_end - name_start - 1;
    if (name_len >= MAX_PROC_NAME) name_len = MAX_PROC_NAME - 1;
    strncpy(pd->name, name_start + 1, name_len);
    pd->name[name_len] = '\0';

    /* Parse fields after ')' */
    char state;
    int ppid, pgrp, session, tty, tpgid;
    unsigned int flags;
    unsigned long minflt, cminflt, majflt, cmajflt;
    unsigned long long utime, stime;
    long long cutime, cstime;
    long priority, nice;
    int num_threads;
    /* We need fields up to rss (field 24) */
    long long itrealvalue, starttime;
    unsigned long vsize;
    long rss;

    int matched = sscanf(name_end + 2,
        "%c %d %d %d %d %d %u %lu %lu %lu %lu %llu %llu %lld %lld %ld %ld %d %lld %llu %lu %ld",
        &state, &ppid, &pgrp, &session, &tty, &tpgid, &flags,
        &minflt, &cminflt, &majflt, &cmajflt,
        &utime, &stime, &cutime, &cstime,
        &priority, &nice, &num_threads, &itrealvalue, &starttime, &vsize, &rss);

    if (matched < 22) return -1;

    pd->pid       = pid;
    pd->state     = state;
    pd->utime     = utime;
    pd->stime     = stime;
    pd->rss_pages = rss;
    pd->threads   = num_threads;

    return 0;
}

/*
 * compare_cpu — Compare two ProcData entries by CPU time (descending)
 * Used by qsort to find top CPU consumers.
 */
static int compare_cpu(const void *a, const void *b)
{
    const ProcData *pa = (const ProcData *)a;
    const ProcData *pb = (const ProcData *)b;
    unsigned long long cpu_a = pa->utime + pa->stime;
    unsigned long long cpu_b = pb->utime + pb->stime;
    if (cpu_b > cpu_a) return 1;
    if (cpu_b < cpu_a) return -1;
    return 0;
}

/*
 * collect_proc_metrics — Main process scanning function
 *
 * 1. Takes snapshot of per-process CPU times
 * 2. Waits briefly
 * 3. Takes second snapshot
 * 4. Calculates per-process CPU usage via delta
 * 5. Sorts and picks top N processes
 */
void collect_proc_metrics(SystemMetrics *metrics)
{
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("Failed to open /proc");
        metrics->proc_ready = 1;
        return;
    }

    /* ── First pass: count states and collect initial CPU times ── */
    ProcData procs1[MAX_SCAN_PROCS];
    int count = 0;
    int total = 0, running = 0, sleeping = 0, zombie = 0, stopped = 0;

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL && count < MAX_SCAN_PROCS) {
        /* Only process numeric directories (PIDs) */
        if (!isdigit(entry->d_name[0])) continue;

        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;

        ProcData pd;
        if (read_proc_stat(pid, &pd) == 0) {
            total++;
            switch (pd.state) {
                case 'R': running++;  break;
                case 'S': case 'D': sleeping++; break;
                case 'Z': zombie++;   break;
                case 'T': case 't': stopped++;  break;
                default: sleeping++; break;
            }
            procs1[count++] = pd;
        }
    }
    closedir(proc_dir);

    /* Record first total CPU time */
    unsigned long long total_cpu_1 = get_total_cpu_time();

    /* Short delay for delta calculation */
    usleep(200000); /* 200ms */

    /* ── Second pass: re-read CPU times ── */
    unsigned long long total_cpu_2 = get_total_cpu_time();
    unsigned long long cpu_delta = total_cpu_2 - total_cpu_1;
    if (cpu_delta == 0) cpu_delta = 1;

    ProcData procs2[MAX_SCAN_PROCS];
    for (int i = 0; i < count; i++) {
        ProcData pd2;
        if (read_proc_stat(procs1[i].pid, &pd2) == 0) {
            /* Calculate per-process CPU percentage */
            unsigned long long proc_delta = 
                (pd2.utime + pd2.stime) - (procs1[i].utime + procs1[i].stime);
            pd2.utime = proc_delta; /* Reuse utime field to store CPU % * 100 */
            pd2.stime = 0;
            procs2[i] = pd2;
        } else {
            procs2[i] = procs1[i];
            procs2[i].utime = 0;
            procs2[i].stime = 0;
        }
    }

    /* Sort by CPU usage (descending) */
    qsort(procs2, count, sizeof(ProcData), compare_cpu);

    /* ── Store results in shared metrics ── */
    metrics->total_processes    = total;
    metrics->running_processes  = running;
    metrics->sleeping_processes = sleeping;
    metrics->zombie_processes   = zombie;
    metrics->stopped_processes  = stopped;

    /* Page size for memory calculation */
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) page_size = 4096;

    /* Copy top processes */
    int top_count = (count < MAX_TOP_PROCS) ? count : MAX_TOP_PROCS;
    metrics->top_proc_count = top_count;

    for (int i = 0; i < top_count; i++) {
        metrics->top_procs[i].pid        = procs2[i].pid;
        strncpy(metrics->top_procs[i].name, procs2[i].name, MAX_PROC_NAME - 1);
        metrics->top_procs[i].name[MAX_PROC_NAME - 1] = '\0';
        metrics->top_procs[i].state      = procs2[i].state;
        metrics->top_procs[i].cpu_percent = (float)procs2[i].utime / cpu_delta * 100.0;
        metrics->top_procs[i].mem_kb     = procs2[i].rss_pages * page_size / 1024;
        metrics->top_procs[i].threads    = procs2[i].threads;
    }

    /* Signal that process data is ready */
    metrics->proc_ready = 1;
}
