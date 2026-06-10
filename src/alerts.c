/*
 * alerts.c — Alert System Implementation
 * 
 * Evaluates system metrics against configurable thresholds defined
 * in shared_data.h. Generates structured alert entries with severity
 * levels (WARNING/CRITICAL) for display on the dashboard and logging.
 *
 * OS Concepts: Conditional logic, system monitoring thresholds
 */

#include <stdio.h>
#include <string.h>
#include "alerts.h"

/* ANSI Color codes */
#define COLOR_YELLOW "\033[33m"
#define COLOR_RED    "\033[31m"
#define COLOR_RESET  "\033[0m"

/*
 * add_alert — Helper to add a new alert to the state
 */
static void add_alert(AlertState *state, AlertLevel level,
                      const char *category, const char *msg)
{
    if (state->count >= MAX_ALERTS) return;

    Alert *a = &state->alerts[state->count];
    a->level = level;
    strncpy(a->category, category, sizeof(a->category) - 1);
    a->category[sizeof(a->category) - 1] = '\0';
    strncpy(a->message, msg, MAX_ALERT_MSG - 1);
    a->message[MAX_ALERT_MSG - 1] = '\0';
    state->count++;
}

/*
 * check_alerts — Evaluate all metrics against thresholds
 *
 * Checks CPU, memory, disk, swap usage percentages and zombie process
 * counts. Generates WARNING or CRITICAL alerts as appropriate.
 */
int check_alerts(const SystemMetrics *metrics, AlertState *state)
{
    char buf[MAX_ALERT_MSG];
    state->count = 0;

    /* ── CPU Alerts ── */
    if (metrics->cpu_usage_percent >= THRESH_CPU_CRIT) {
        snprintf(buf, sizeof(buf), "CPU usage at %.1f%% — exceeds %.0f%% critical threshold!",
                 metrics->cpu_usage_percent, THRESH_CPU_CRIT);
        add_alert(state, ALERT_CRITICAL, "CPU", buf);
    } else if (metrics->cpu_usage_percent >= THRESH_CPU_WARN) {
        snprintf(buf, sizeof(buf), "CPU usage at %.1f%% — exceeds %.0f%% warning threshold",
                 metrics->cpu_usage_percent, THRESH_CPU_WARN);
        add_alert(state, ALERT_WARNING, "CPU", buf);
    }

    /* ── Memory Alerts ── */
    if (metrics->mem_usage_percent >= THRESH_MEM_CRIT) {
        snprintf(buf, sizeof(buf), "Memory usage at %.1f%% — exceeds %.0f%% critical threshold!",
                 metrics->mem_usage_percent, THRESH_MEM_CRIT);
        add_alert(state, ALERT_CRITICAL, "MEM", buf);
    } else if (metrics->mem_usage_percent >= THRESH_MEM_WARN) {
        snprintf(buf, sizeof(buf), "Memory usage at %.1f%% — exceeds %.0f%% warning threshold",
                 metrics->mem_usage_percent, THRESH_MEM_WARN);
        add_alert(state, ALERT_WARNING, "MEM", buf);
    }

    /* ── Disk Alerts ── */
    if (metrics->disk_usage_percent >= THRESH_DISK_CRIT) {
        snprintf(buf, sizeof(buf), "Disk usage at %.1f%% — exceeds %.0f%% critical threshold!",
                 metrics->disk_usage_percent, THRESH_DISK_CRIT);
        add_alert(state, ALERT_CRITICAL, "DISK", buf);
    } else if (metrics->disk_usage_percent >= THRESH_DISK_WARN) {
        snprintf(buf, sizeof(buf), "Disk usage at %.1f%% — exceeds %.0f%% warning threshold",
                 metrics->disk_usage_percent, THRESH_DISK_WARN);
        add_alert(state, ALERT_WARNING, "DISK", buf);
    }

    /* ── Swap Alerts ── */
    if (metrics->swap_total_kb > 0) {
        if (metrics->swap_usage_percent >= THRESH_SWAP_CRIT) {
            snprintf(buf, sizeof(buf), "Swap usage at %.1f%% — exceeds %.0f%% critical threshold!",
                     metrics->swap_usage_percent, THRESH_SWAP_CRIT);
            add_alert(state, ALERT_CRITICAL, "SWAP", buf);
        } else if (metrics->swap_usage_percent >= THRESH_SWAP_WARN) {
            snprintf(buf, sizeof(buf), "Swap usage at %.1f%% — exceeds %.0f%% warning threshold",
                     metrics->swap_usage_percent, THRESH_SWAP_WARN);
            add_alert(state, ALERT_WARNING, "SWAP", buf);
        }
    }

    /* ── Zombie Process Alerts ── */
    if (metrics->zombie_processes >= THRESH_ZOMBIE_CRIT) {
        snprintf(buf, sizeof(buf), "%d zombie processes detected — exceeds critical threshold of %d!",
                 metrics->zombie_processes, THRESH_ZOMBIE_CRIT);
        add_alert(state, ALERT_CRITICAL, "PROC", buf);
    } else if (metrics->zombie_processes >= THRESH_ZOMBIE_WARN) {
        snprintf(buf, sizeof(buf), "%d zombie process(es) detected",
                 metrics->zombie_processes);
        add_alert(state, ALERT_WARNING, "PROC", buf);
    }

    /* ── High Load Average Alert ── */
    if (metrics->load_avg_1 > (float)metrics->cpu_core_count * 2.0) {
        snprintf(buf, sizeof(buf), "Load average %.2f exceeds 2x CPU cores (%d)",
                 metrics->load_avg_1, metrics->cpu_core_count);
        add_alert(state, ALERT_CRITICAL, "LOAD", buf);
    } else if (metrics->load_avg_1 > (float)metrics->cpu_core_count) {
        snprintf(buf, sizeof(buf), "Load average %.2f exceeds CPU core count (%d)",
                 metrics->load_avg_1, metrics->cpu_core_count);
        add_alert(state, ALERT_WARNING, "LOAD", buf);
    }

    return state->count;
}

/*
 * get_alert_color — Return ANSI color escape code for alert level
 */
const char* get_alert_color(AlertLevel level)
{
    switch (level) {
        case ALERT_WARNING:  return COLOR_YELLOW;
        case ALERT_CRITICAL: return COLOR_RED;
        default:             return COLOR_RESET;
    }
}

/*
 * get_alert_label — Return text label for alert level
 */
const char* get_alert_label(AlertLevel level)
{
    switch (level) {
        case ALERT_WARNING:  return "WARNING";
        case ALERT_CRITICAL: return "CRITICAL";
        default:             return "INFO";
    }
}
