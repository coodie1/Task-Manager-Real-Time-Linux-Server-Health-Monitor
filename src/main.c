

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include "shared_data.h"
#include "cpu_monitor.h"
#include "mem_monitor.h"
#include "disk_monitor.h"
#include "proc_monitor.h"
#include "dashboard.h"
#include "alerts.h"
#include "logger.h"

/* ── Global State ── */
static int          shm_id      = -1;       /* Shared memory segment ID */
static SystemMetrics *shm_ptr   = NULL;     /* Shared memory pointer */
static volatile int  running    = 1;        /* Main loop control flag */
static volatile int  gen_report = 0;        /* Report generation flag */
static char          base_dir[512] = "";    /* Project base directory */

/* ── Forward Declarations ── */
static void setup_shared_memory(void);
static void cleanup_shared_memory(void);
static void setup_signal_handlers(void);
static void signal_handler(int sig);
static void alarm_handler(int sig);
static void read_system_info(SystemMetrics *metrics);
static void generate_report(void);
static void create_directories(void);

/*
 * main — Entry point
 *
 * Sets up shared memory, signal handlers, logging, and enters
 * the main monitoring loop.
 */
int main(int argc, char *argv[])
{
    (void)argc;

    /* Determine base directory (where the executable is) */
    /* Find the directory containing the executable */
    char *last_slash = strrchr(argv[0], '/');
    if (last_slash) {
        int len = last_slash - argv[0];
        strncpy(base_dir, argv[0], len);
        base_dir[len] = '\0';
    } else {
        /* Executable in current directory */
        if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
            strcpy(base_dir, ".");
        }
    }

    /* Create necessary directories */
    create_directories();

    /* Initialize subsystems */
    setup_shared_memory();
    setup_signal_handlers();

    /* Initialize logger */
    char log_dir[600];
    snprintf(log_dir, sizeof(log_dir), "%s/logs", base_dir);
    if (logger_init(log_dir) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        cleanup_shared_memory();
        return 1;
    }

    log_message(LOG_INFO, "Server Health Monitor started.");
    printf("Server Health Monitor starting...\n");
    sleep(1);

    /* Hide cursor for cleaner display */
    hide_cursor();
    clear_screen();

    /* Set alarm for hourly report generation */
    alarm(3600);

    /* Alert state */
    AlertState alert_state;
//main loop
    while (running) {

        /* Reset synchronization flags */
        shm_ptr->cpu_ready  = 0;
        shm_ptr->mem_ready  = 0;
        shm_ptr->disk_ready = 0;
        shm_ptr->proc_ready = 0;

        /* Read hostname and uptime (done by parent, lightweight) */
        read_system_info(shm_ptr);

        /* ── Fork Child Process 1: CPU Monitor ── */
        pid_t pid_cpu = fork();
        if (pid_cpu < 0) {
            perror("fork failed for CPU monitor");
        } else if (pid_cpu == 0) {
            /* Child process: collect CPU metrics and exit */
            collect_cpu_metrics(shm_ptr);
            _exit(0);
        }

        /* ── Fork Child Process 2: Memory Monitor ── */
        pid_t pid_mem = fork();
        if (pid_mem < 0) {
            perror("fork failed for Memory monitor");
        } else if (pid_mem == 0) {
            /* Child process: collect memory metrics and exit */
            collect_mem_metrics(shm_ptr);
            _exit(0);
        }

        /* ── Fork Child Process 3: Disk Monitor ── */
        pid_t pid_disk = fork();
        if (pid_disk < 0) {
            perror("fork failed for Disk monitor");
        } else if (pid_disk == 0) {
            /* Child process: collect disk metrics and exit */
            collect_disk_metrics(shm_ptr);
            _exit(0);
        }

        /* ── Fork Child Process 4: Process Monitor ── */
        pid_t pid_proc = fork();
        if (pid_proc < 0) {
            perror("fork failed for Process monitor");
        } else if (pid_proc == 0) {
            /* Child process: collect process metrics and exit */
            collect_proc_metrics(shm_ptr);
            _exit(0);
        }

        /* ── Parent: Wait for all 4 children to complete ── */
        int status;
        if (pid_cpu > 0)  waitpid(pid_cpu,  &status, 0);
        if (pid_mem > 0)  waitpid(pid_mem,  &status, 0);
        if (pid_disk > 0) waitpid(pid_disk, &status, 0);
        if (pid_proc > 0) waitpid(pid_proc, &status, 0);

        /* ── Check alerts against thresholds ── */
        check_alerts(shm_ptr, &alert_state);

        /* ── Render the dashboard ── */
        render_dashboard(shm_ptr, &alert_state);

        /* ── Log metrics and alerts ── */
        log_metrics(shm_ptr);
        if (alert_state.count > 0) {
            log_alerts(&alert_state);
        }

        /* ── Check if hourly report should be generated ── */
        if (gen_report) {
            gen_report = 0;
            generate_report();
            alarm(3600);  /* Reset alarm for next hour */
        }

        /* ── Sleep until next refresh cycle ── */
        sleep(REFRESH_INTERVAL);
    }

 
    show_cursor();
    printf("\n\033[0m");  /* Reset colors */
    log_message(LOG_INFO, "Server Health Monitor stopped. Cleaning up...");
    logger_close();
    cleanup_shared_memory();

    printf("\n  Server Health Monitor stopped.\n");
    printf("  Log files: %s/logs/\n", base_dir);
    printf("  Reports:   %s/reports/\n\n", base_dir);

    return 0;
}

/*
 * setup_shared_memory — Create and attach shared memory segment
 *
 * Uses shmget() to create a shared memory segment identified by SHM_KEY.
 * Uses shmat() to attach the segment to this process's address space.
 * The segment is shared with all forked child processes (inherited).
  */
static void setup_shared_memory(void)
{
    /* Create shared memory segment */
    shm_id = shmget(SHM_KEY, sizeof(SystemMetrics), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget failed");
        fprintf(stderr, "Error: Could not create shared memory segment.\n");
        fprintf(stderr, "Try: ipcrm -M 0x%X\n", SHM_KEY);
        exit(1);
    }

    /* Attach shared memory to process address space */
    shm_ptr = (SystemMetrics *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (SystemMetrics *)-1) {
        perror("shmat failed");
        exit(1);
    }

    /* Initialize to zero */
    memset(shm_ptr, 0, sizeof(SystemMetrics));
}

/*
 * cleanup_shared_memory — Detach and remove shared memory segment
 *
 * Called during graceful shutdown to free system resources.
 */
static void cleanup_shared_memory(void)
{
    if (shm_ptr && shm_ptr != (SystemMetrics *)-1) {
        shmdt(shm_ptr);
        shm_ptr = NULL;
    }
    if (shm_id >= 0) {
        shmctl(shm_id, IPC_RMID, NULL);
        shm_id = -1;
    }
}

/*
 * setup_signal_handlers — Register handlers for SIGINT, SIGTERM, SIGALRM
 *
 * SIGINT/SIGTERM: Set running=0 for graceful shutdown
 * SIGALRM: Set gen_report=1 to trigger hourly report generation
  */
static void setup_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Handler for SIGALRM (hourly report trigger) */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sa.sa_flags = SA_RESTART;  /* Restart interrupted system calls */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

}


static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

/*
 * alarm_handler — Handle SIGALRM for hourly report generation
 */
static void alarm_handler(int sig)
{
    (void)sig;
    gen_report = 1;
}

/*
 * read_system_info — Read hostname and uptime from /proc
 *
 * /proc/sys/kernel/hostname: contains the system hostname
 * /proc/uptime: contains uptime in seconds (floating point)
 */
static void read_system_info(SystemMetrics *metrics)
{
    /* Hostname */
    FILE *fp = fopen("/proc/sys/kernel/hostname", "r");
    if (fp) {
        if (fgets(metrics->hostname, sizeof(metrics->hostname), fp)) {
            /* Remove trailing newline */
            char *nl = strchr(metrics->hostname, '\n');
            if (nl) *nl = '\0';
        }
        fclose(fp);
    } else {
        strcpy(metrics->hostname, "unknown");
    }

    /* Uptime */
    fp = fopen("/proc/uptime", "r");
    if (fp) {
        double uptime;
        if (fscanf(fp, "%lf", &uptime) == 1) {
            metrics->uptime_seconds = (long)uptime;
        }
        fclose(fp);
    }
}

/*
 * generate_report — Fork a child to execute the report generator script
 *
 * Forks a child process that uses execl() to run the Bash report
 * generator script. The parent does not wait — the report runs
 * in the background.
 *
 * 
 */
static void generate_report(void)
{
    char script_path[600];
    snprintf(script_path, sizeof(script_path), "%s/scripts/report_generator.sh", base_dir);

    char log_path[600];
    snprintf(log_path, sizeof(log_path), "%s/logs", base_dir);

    char report_path[600];
    snprintf(report_path, sizeof(report_path), "%s/reports", base_dir);

    log_message(LOG_INFO, "Generating hourly report...");

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed for report generation");
        return;
    } else if (pid == 0) {
        /* Child: execute report generator script */
        execl("/bin/bash", "bash", script_path, log_path, report_path, NULL);
        /* If exec fails */
        perror("execl failed for report generator");
        _exit(1);
    }
    /* Parent: don't wait, report runs in background */
    /* We'll reap it with waitpid in the main loop or ignore */
}

/*
 * create_directories — Ensure logs/ and reports/ directories exist
 */
static void create_directories(void)
{
    char path[600];

    snprintf(path, sizeof(path), "%s/logs", base_dir);
    mkdir(path, 0755);

    snprintf(path, sizeof(path), "%s/reports", base_dir);
    mkdir(path, 0755);
}
