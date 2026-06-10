/*
 * logger.c — Timestamped Event Logging Implementation
 * 
 * Creates daily log files in the format: monitor_YYYYMMDD.log
 * Each entry has a timestamp, log level, and message.
 * Supports logging metric snapshots and alert events.
 *
 * OS Concepts: File I/O (Lab 4), timestamps, directory management
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "logger.h"

/* Static state for the logger */
static FILE *log_fp = NULL;
static char  log_dir_path[256] = "";
static char  current_log_file[512] = "";
static int   current_day = -1;

/*
 * get_timestamp — Format current time as [YYYY-MM-DD HH:MM:SS]
 */
static void get_timestamp(char *buf, size_t bufsize)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, bufsize, "%Y-%m-%d %H:%M:%S", tm_info);
}

/*
 * get_level_string — Convert log level to string
 */
static const char* get_level_string(LogLevel level)
{
    switch (level) {
        case LOG_INFO:     return "INFO";
        case LOG_WARNING:  return "WARNING";
        case LOG_CRITICAL: return "CRITICAL";
        case LOG_ERROR:    return "ERROR";
        default:           return "UNKNOWN";
    }
}

/*
 * ensure_log_file — Create/rotate log file based on current date
 * 
 * If the day has changed since the last call, close the old file
 * and open a new one with today's date.
 */
static int ensure_log_file(void)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    /* Check if we need to rotate to a new daily file */
    if (tm_info->tm_mday != current_day || log_fp == NULL) {
        if (log_fp) {
            fclose(log_fp);
            log_fp = NULL;
        }

        snprintf(current_log_file, sizeof(current_log_file),
                 "%s/monitor_%04d%02d%02d.log",
                 log_dir_path,
                 tm_info->tm_year + 1900,
                 tm_info->tm_mon + 1,
                 tm_info->tm_mday);

        log_fp = fopen(current_log_file, "a");
        if (!log_fp) {
            perror("Failed to open log file");
            return -1;
        }

        current_day = tm_info->tm_mday;

        /* Write a header for new log files */
        char ts[32];
        get_timestamp(ts, sizeof(ts));
        fprintf(log_fp, "========================================\n");
        fprintf(log_fp, "  Server Health Monitor Log\n");
        fprintf(log_fp, "  Started: %s\n", ts);
        fprintf(log_fp, "========================================\n\n");
        fflush(log_fp);
    }

    return 0;
}

/*
 * logger_init — Initialize the logging system
 */
int logger_init(const char *log_dir)
{
    strncpy(log_dir_path, log_dir, sizeof(log_dir_path) - 1);
    log_dir_path[sizeof(log_dir_path) - 1] = '\0';

    /* Create logs directory if it doesn't exist */
    struct stat st;
    if (stat(log_dir_path, &st) == -1) {
        if (mkdir(log_dir_path, 0755) == -1) {
            perror("Failed to create logs directory");
            return -1;
        }
    }

    return ensure_log_file();
}

/*
 * logger_close — Close the current log file
 */
void logger_close(void)
{
    if (log_fp) {
        char ts[32];
        get_timestamp(ts, sizeof(ts));
        fprintf(log_fp, "\n[%s] [INFO] Monitor stopped.\n", ts);
        fclose(log_fp);
        log_fp = NULL;
    }
    current_day = -1;
}

/*
 * log_metrics — Write a metrics snapshot
 * 
 * Writes a single line with all key metrics for easy parsing
 * by the report generator script.
 */
void log_metrics(const SystemMetrics *metrics)
{
    if (ensure_log_file() != 0) return;

    char ts[32];
    get_timestamp(ts, sizeof(ts));

    fprintf(log_fp,
        "[%s] [METRIC] CPU:%.1f%% MEM:%.1f%% DISK:%.1f%% SWAP:%.1f%% "
        "LOAD:%.2f PROCS:%d RUN:%d ZMB:%d "
        "MEM_USED:%ldMB/%ldMB DISK_USED:%ldMB/%ldMB "
        "DISK_R:%ldKB/s DISK_W:%ldKB/s\n",
        ts,
        metrics->cpu_usage_percent,
        metrics->mem_usage_percent,
        metrics->disk_usage_percent,
        metrics->swap_usage_percent,
        metrics->load_avg_1,
        metrics->total_processes,
        metrics->running_processes,
        metrics->zombie_processes,
        metrics->mem_used_kb / 1024, metrics->mem_total_kb / 1024,
        metrics->disk_used_mb, metrics->disk_total_mb,
        metrics->disk_read_speed_kb,
        metrics->disk_write_speed_kb);

    fflush(log_fp);
}

/*
 * log_alerts — Write alert entries
 */
void log_alerts(const AlertState *alerts)
{
    if (ensure_log_file() != 0) return;
    if (alerts->count == 0) return;

    char ts[32];
    get_timestamp(ts, sizeof(ts));

    for (int i = 0; i < alerts->count; i++) {
        const Alert *a = &alerts->alerts[i];
        fprintf(log_fp, "[%s] [%s] [%s] %s\n",
                ts, get_alert_label(a->level), a->category, a->message);
    }

    fflush(log_fp);
}

/*
 * log_message — Write a generic message
 */
void log_message(LogLevel level, const char *message)
{
    if (ensure_log_file() != 0) return;

    char ts[32];
    get_timestamp(ts, sizeof(ts));

    fprintf(log_fp, "[%s] [%s] %s\n", ts, get_level_string(level), message);
    fflush(log_fp);
}

/*
 * get_log_filepath — Return path to current log file
 */
const char* get_log_filepath(void)
{
    return current_log_file;
}
