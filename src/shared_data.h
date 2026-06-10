/*
 * shared_data.h — Shared Memory Data Structures
 * 
 * Defines the SystemMetrics struct shared between parent and child processes
 * via POSIX shared memory (shmget/shmat). Each child monitor process writes
 * its metrics into this shared segment, and the parent reads them to render
 * the dashboard.
 *
 * OS Concepts: Shared Memory IPC (Lab 7-8)
 */

#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <sys/types.h>

/* Maximum number of top processes to display */
#define MAX_TOP_PROCS 10

/* Maximum length of a process name */
#define MAX_PROC_NAME 64

/* Dashboard refresh interval in seconds */
#define REFRESH_INTERVAL 2

/* Alert threshold levels */
#define THRESH_CPU_WARN    5.0
#define THRESH_CPU_CRIT    15.0
#define THRESH_MEM_WARN    30.0
#define THRESH_MEM_CRIT    40.0
#define THRESH_DISK_WARN   80.0
#define THRESH_DISK_CRIT   95.0
#define THRESH_SWAP_WARN   50.0
#define THRESH_SWAP_CRIT   80.0
#define THRESH_ZOMBIE_WARN 1
#define THRESH_ZOMBIE_CRIT 5

/* Alert severity levels */
typedef enum {
    ALERT_NONE = 0,
    ALERT_WARNING,
    ALERT_CRITICAL
} AlertLevel;

/* Information about a single process */
typedef struct {
    int    pid;
    char   name[MAX_PROC_NAME];
    char   state;           /* R=running, S=sleeping, Z=zombie, T=stopped, etc. */
    float  cpu_percent;
    long   mem_kb;
    int    threads;
} ProcessInfo;

/* Main shared data structure between parent and child processes */
typedef struct {
    /* ── CPU Metrics ── */
    float  cpu_usage_percent;
    float  cpu_user_percent;
    float  cpu_system_percent;
    float  cpu_idle_percent;
    float  cpu_iowait_percent;
    int    cpu_core_count;
    float  load_avg_1, load_avg_5, load_avg_15;

    /* ── Memory Metrics ── */
    long   mem_total_kb;
    long   mem_used_kb;
    long   mem_free_kb;
    long   mem_available_kb;
    long   mem_buffers_kb;
    long   mem_cached_kb;
    float  mem_usage_percent;

    /* ── Swap Metrics ── */
    long   swap_total_kb;
    long   swap_free_kb;
    long   swap_used_kb;
    float  swap_usage_percent;

    /* ── Disk Metrics ── */
    long   disk_total_mb;
    long   disk_used_mb;
    long   disk_free_mb;
    float  disk_usage_percent;
    long   disk_read_speed_kb;   /* KB/s */
    long   disk_write_speed_kb;  /* KB/s */

    /* ── Process Metrics ── */
    int    total_processes;
    int    running_processes;
    int    sleeping_processes;
    int    zombie_processes;
    int    stopped_processes;
    ProcessInfo top_procs[MAX_TOP_PROCS];
    int    top_proc_count;

    /* ── Synchronization flags ── */
    volatile int cpu_ready;
    volatile int mem_ready;
    volatile int disk_ready;
    volatile int proc_ready;

    /* ── System Info ── */
    char   hostname[64];
    long   uptime_seconds;
    int    logged_in_users;

} SystemMetrics;

/* Shared memory key (arbitrary, unique for this application) */
#define SHM_KEY 0x48454C54  /* "HELT" */

#endif /* SHARED_DATA_H */
