/*
 * dashboard.c — CLI Dashboard Renderer Implementation
 * 
 * Renders a professional-looking CLI dashboard using ANSI escape codes
 * and Unicode box-drawing characters. Features:
 *   - Color-coded progress bars (green/yellow/red by threshold)
 *   - System info header (hostname, uptime, datetime)
 *   - CPU/Memory/Disk/Swap gauges
 *   - Top processes table
 *   - Alert panel
 *   - Process state summary
 *
 * OS Concepts: Terminal I/O, escape sequences, formatted output
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "dashboard.h"

/* ── ANSI Color/Style Codes ── */
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define UNDERLINE   "\033[4m"
#define BLINK       "\033[5m"

/* Foreground colors */
#define FG_BLACK    "\033[30m"
#define FG_RED      "\033[31m"
#define FG_GREEN    "\033[32m"
#define FG_YELLOW   "\033[33m"
#define FG_BLUE     "\033[34m"
#define FG_MAGENTA  "\033[35m"
#define FG_CYAN     "\033[36m"
#define FG_WHITE    "\033[37m"

/* Bright foreground colors */
#define FG_BRED     "\033[91m"
#define FG_BGREEN   "\033[92m"
#define FG_BYELLOW  "\033[93m"
#define FG_BBLUE    "\033[94m"
#define FG_BMAGENTA "\033[95m"
#define FG_BCYAN    "\033[96m"
#define FG_BWHITE   "\033[97m"

/* Background colors */
#define BG_BLACK    "\033[40m"
#define BG_RED      "\033[41m"
#define BG_GREEN    "\033[42m"
#define BG_BLUE     "\033[44m"
#define BG_CYAN     "\033[46m"

/* Dashboard width */
#define DASH_WIDTH 76

/* Unicode box-drawing characters */
#define BOX_TL  "╔"
#define BOX_TR  "╗"
#define BOX_BL  "╚"
#define BOX_BR  "╝"
#define BOX_H   "═"
#define BOX_V   "║"
#define BOX_LT  "╠"
#define BOX_RT  "╣"
#define BOX_HT  "─"
#define BOX_VT  "│"

/* Progress bar characters */
#define BAR_FULL  "█"
#define BAR_MED   "▓"
#define BAR_LOW   "░"
#define BAR_EMPTY "░"

/*
 * get_bar_color — Choose color based on percentage and thresholds
 */
static const char* get_bar_color(float percent, float warn, float crit)
{
    if (percent >= crit) return FG_RED;
    if (percent >= warn) return FG_YELLOW;
    return FG_GREEN;
}

/*
 * format_bytes — Convert KB to human-readable format
 */
static void format_bytes(long kb, char *buf, size_t bufsize)
{
    if (kb >= 1048576) {  /* >= 1 GB */
        snprintf(buf, bufsize, "%.1f GB", (double)kb / 1048576.0);
    } else if (kb >= 1024) {  /* >= 1 MB */
        snprintf(buf, bufsize, "%.1f MB", (double)kb / 1024.0);
    } else {
        snprintf(buf, bufsize, "%ld KB", kb);
    }
}

/*
 * format_uptime — Convert seconds to human-readable uptime
 */
static void format_uptime(long seconds, char *buf, size_t bufsize)
{
    int days  = seconds / 86400;
    int hours = (seconds % 86400) / 3600;
    int mins  = (seconds % 3600) / 60;

    if (days > 0) {
        snprintf(buf, bufsize, "%dd %dh %dm", days, hours, mins);
    } else if (hours > 0) {
        snprintf(buf, bufsize, "%dh %dm", hours, mins);
    } else {
        snprintf(buf, bufsize, "%dm %lds", mins, seconds % 60);
    }
}

/*
 * print_horizontal_line — Print a horizontal divider
 */
static void print_horizontal_line(const char *left, const char *right)
{
    printf("  %s%s%s", FG_CYAN, left, RESET);
    for (int i = 0; i < DASH_WIDTH - 2; i++) {
        printf("%s%s%s", FG_CYAN, BOX_H, RESET);
    }
    printf("%s%s%s\n", FG_CYAN, right, RESET);
}

/*
 * print_empty_line — Print an empty bordered line
 */
static void print_empty_line(void)
{
    printf("  %s%s%s", FG_CYAN, BOX_V, RESET);
    for (int i = 0; i < DASH_WIDTH - 2; i++) printf(" ");
    printf("%s%s%s\n", FG_CYAN, BOX_V, RESET);
}

/*
 * print_progress_bar — Render a colored progress bar with label
 *
 * Layout: "  ║  Label    ████████████░░░░░░░░  XX.X%  Used/Total    ║"
 */
static void print_progress_bar(const char *label, float percent,
                                float warn_thresh, float crit_thresh,
                                const char *detail)
{
    int bar_width = 25;
    int filled = (int)(percent / 100.0 * bar_width);
    if (filled > bar_width) filled = bar_width;
    if (filled < 0) filled = 0;

    const char *color = get_bar_color(percent, warn_thresh, crit_thresh);

    /* Status indicator */
    const char *status = "";
    const char *status_color = RESET;
    if (percent >= crit_thresh) {
        status = " !!CRIT";
        status_color = FG_RED;
    } else if (percent >= warn_thresh) {
        status = " !WARN";
        status_color = FG_YELLOW;
    }

    printf("  %s%s%s  %s%-10s%s ", FG_CYAN, BOX_V, RESET, BOLD, label, RESET);

    /* Draw the bar */
    printf("%s", color);
    for (int i = 0; i < filled; i++) printf("%s", BAR_FULL);
    printf("%s", DIM);
    for (int i = filled; i < bar_width; i++) printf("%s", BAR_EMPTY);
    printf("%s", RESET);

    /* Percentage + detail */
    printf("  %s%s%5.1f%%%s", BOLD, color, percent, RESET);
    printf("  %s%-14s%s", DIM, detail, RESET);

    /* Status badge */
    int content_len = 10 + 1 + bar_width + 7 + 2 + 14;
    int padding = DASH_WIDTH - 4 - content_len - (int)strlen(status);
    if (padding < 0) padding = 0;
    for (int i = 0; i < padding; i++) printf(" ");
    printf("%s%s%s%s", BOLD, status_color, status, RESET);

    printf(" %s%s%s\n", FG_CYAN, BOX_V, RESET);
}

/*
 * clear_screen — Clear terminal and move cursor to top-left
 */
void clear_screen(void)
{
    printf("\033[2J\033[H");
    fflush(stdout);
}

/*
 * hide_cursor / show_cursor — Terminal cursor visibility
 */
void hide_cursor(void) { printf("\033[?25l"); fflush(stdout); }
void show_cursor(void) { printf("\033[?25h"); fflush(stdout); }

/*
 * render_dashboard — Main dashboard rendering function
 */
void render_dashboard(const SystemMetrics *metrics, const AlertState *alerts)
{
    /* Move cursor to top-left instead of clearing (reduces flicker) */
    printf("\033[H");

    /* Get current time */
    char time_str[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    /* Format uptime */
    char uptime_str[32];
    format_uptime(metrics->uptime_seconds, uptime_str, sizeof(uptime_str));

    /* ══════════════════════ HEADER ══════════════════════ */
    printf("\n");
    print_horizontal_line(BOX_TL, BOX_TR);

    /* Title */
    printf("  %s%s%s  %s%s  LINUX SERVER HEALTH MONITOR%s",
           FG_CYAN, BOX_V, RESET, BOLD, FG_BGREEN, RESET);
    int title_len = 36;
    for (int i = title_len; i < DASH_WIDTH - 3; i++) printf(" ");
    printf("%s%s%s\n", FG_CYAN, BOX_V, RESET);

    /* Subtitle: hostname, uptime, time */
    printf("  %s%s%s  %sHost:%s %-12s  %sUptime:%s %-14s  %s%s%s",
           FG_CYAN, BOX_V, RESET,
           FG_BCYAN, RESET, metrics->hostname,
           FG_BCYAN, RESET, uptime_str,
           DIM, time_str, RESET);
    int sub_len = 7 + 12 + 2 + 9 + 14 + 2 + 19;
    for (int i = sub_len; i < DASH_WIDTH - 3; i++) printf(" ");
    printf("%s%s%s\n", FG_CYAN, BOX_V, RESET);

    /* ══════════════════ SYSTEM GAUGES ══════════════════ */
    print_horizontal_line(BOX_LT, BOX_RT);

    /* Section label */
    printf("  %s%s%s  %s%s SYSTEM RESOURCES%s",
           FG_CYAN, BOX_V, RESET, BOLD, FG_BBLUE, RESET);
    int sec_len = 20;
    for (int i = sec_len; i < DASH_WIDTH - 3; i++) printf(" ");
    printf("%s%s%s\n", FG_CYAN, BOX_V, RESET);

    print_empty_line();

    /* CPU bar */
    char cpu_detail[32];
    snprintf(cpu_detail, sizeof(cpu_detail), "Cores: %d", metrics->cpu_core_count);
    print_progress_bar("CPU", metrics->cpu_usage_percent,
                       THRESH_CPU_WARN, THRESH_CPU_CRIT, cpu_detail);

    /* Memory bar */
    char mem_detail[64];
    char mem_used_str[32], mem_total_str[32];
    format_bytes(metrics->mem_used_kb, mem_used_str, sizeof(mem_used_str));
    format_bytes(metrics->mem_total_kb, mem_total_str, sizeof(mem_total_str));
    snprintf(mem_detail, sizeof(mem_detail), "%s/%s", mem_used_str, mem_total_str);
    print_progress_bar("Memory", metrics->mem_usage_percent,
                       THRESH_MEM_WARN, THRESH_MEM_CRIT, mem_detail);

    /* Disk bar */
    char disk_detail[64];
    snprintf(disk_detail, sizeof(disk_detail), "%ldMB/%ldMB",
             metrics->disk_used_mb, metrics->disk_total_mb);
    print_progress_bar("Disk", metrics->disk_usage_percent,
                       THRESH_DISK_WARN, THRESH_DISK_CRIT, disk_detail);

    /* Swap bar */
    char swap_detail[64];
    char swap_used_str[32], swap_total_str[32];
    format_bytes(metrics->swap_used_kb, swap_used_str, sizeof(swap_used_str));
    format_bytes(metrics->swap_total_kb, swap_total_str, sizeof(swap_total_str));
    snprintf(swap_detail, sizeof(swap_detail), "%s/%s", swap_used_str, swap_total_str);
    print_progress_bar("Swap", metrics->swap_usage_percent,
                       THRESH_SWAP_WARN, THRESH_SWAP_CRIT, swap_detail);

    print_empty_line();

    /* ── CPU Breakdown + Load Averages ── */
    printf("  %s%s%s  %sUser:%s %5.1f%%  %sSys:%s %5.1f%%  %sIdle:%s %5.1f%%  "
           "%sIOw:%s %4.1f%%  %s%sLoad:%s %.2f %.2f %.2f",
           FG_CYAN, BOX_V, RESET,
           FG_GREEN, RESET, metrics->cpu_user_percent,
           FG_YELLOW, RESET, metrics->cpu_system_percent,
           FG_BCYAN, RESET, metrics->cpu_idle_percent,
           FG_MAGENTA, RESET, metrics->cpu_iowait_percent,
           BOLD, FG_BWHITE, RESET,
           metrics->load_avg_1, metrics->load_avg_5, metrics->load_avg_15);
    /* Calculate remaining space */
    printf("  %s%s%s\n", FG_CYAN, BOX_V, RESET);

    /* ══════════════════ DISK I/O ══════════════════ */
    printf("  %s%s%s  %sDisk I/O:%s  Read: %s%ld KB/s%s  Write: %s%ld KB/s%s",
           FG_CYAN, BOX_V, RESET,
           FG_BBLUE, RESET,
           FG_GREEN, metrics->disk_read_speed_kb, RESET,
           FG_YELLOW, metrics->disk_write_speed_kb, RESET);
    int dio_len = 50;
    for (int i = dio_len; i < DASH_WIDTH - 3; i++) printf(" ");
    printf("%s%s%s\n", FG_CYAN, BOX_V, RESET);

    /* ══════════════════ TOP PROCESSES ══════════════════ */
    print_horizontal_line(BOX_LT, BOX_RT);

    printf("  %s%s%s  %s%s TOP PROCESSES%s      (Total: %d  Running: %d  Sleeping: %d  Zombie: %s%d%s  Stopped: %d)",
           FG_CYAN, BOX_V, RESET, BOLD, FG_BBLUE, RESET,
           metrics->total_processes, metrics->running_processes,
           metrics->sleeping_processes,
           metrics->zombie_processes > 0 ? FG_RED : "", metrics->zombie_processes,
           metrics->zombie_processes > 0 ? RESET : "",
           metrics->stopped_processes);
    printf("\n");

    print_empty_line();

    /* Table header */
    printf("  %s%s%s  %s%s  %-7s  %-20s  %-5s  %7s  %10s  %5s%s",
           FG_CYAN, BOX_V, RESET, BOLD, FG_BWHITE,
           "PID", "NAME", "STATE", "CPU%", "MEMORY", "THR", RESET);
    int hdr_pad = DASH_WIDTH - 4 - 7 - 2 - 20 - 2 - 5 - 2 - 7 - 2 - 10 - 2 - 5;
    for (int i = 0; i < hdr_pad; i++) printf(" ");
    printf("%s%s%s\n", FG_CYAN, BOX_V, RESET);

    /* Thin separator */
    printf("  %s%s%s  ", FG_CYAN, BOX_V, RESET);
    for (int i = 0; i < DASH_WIDTH - 4; i++) printf("%s%s%s", DIM, BOX_HT, RESET);
    printf(" %s%s%s\n", FG_CYAN, BOX_V, RESET);

    /* Process rows */
    int display_count = metrics->top_proc_count;
    if (display_count > 8) display_count = 8;

    for (int i = 0; i < display_count; i++) {
        const ProcessInfo *p = &metrics->top_procs[i];

        /* Color state indicator */
        const char *state_color;
        const char *state_name;
        switch (p->state) {
            case 'R': state_color = FG_GREEN;  state_name = "RUN";   break;
            case 'S': state_color = FG_BCYAN;  state_name = "SLEEP"; break;
            case 'D': state_color = FG_YELLOW; state_name = "DISK";  break;
            case 'Z': state_color = FG_RED;    state_name = "ZMBI";  break;
            case 'T': state_color = FG_MAGENTA;state_name = "STOP";  break;
            default:  state_color = DIM;       state_name = "OTH";   break;
        }

        /* Format memory */
        char mem_str[32];
        format_bytes(p->mem_kb, mem_str, sizeof(mem_str));

        /* CPU% color */
        const char *cpu_color = FG_GREEN;
        if (p->cpu_percent > 50.0) cpu_color = FG_RED;
        else if (p->cpu_percent > 20.0) cpu_color = FG_YELLOW;

        printf("  %s%s%s  %s  %-7d  %-20.20s  %s%-5s%s  %s%6.1f%%%s  %10s  %5d%s",
               FG_CYAN, BOX_V, RESET, DIM,
               p->pid, p->name,
               state_color, state_name, RESET,
               cpu_color, p->cpu_percent, RESET,
               mem_str, p->threads, RESET);

        int row_pad = DASH_WIDTH - 4 - 7 - 2 - 20 - 2 - 5 - 2 - 7 - 2 - 10 - 2 - 5;
        for (int j = 0; j < row_pad; j++) printf(" ");
        printf("%s%s%s\n", FG_CYAN, BOX_V, RESET);
    }

    /* Fill remaining rows if fewer than 5 processes */
    for (int i = display_count; i < 5; i++) {
        print_empty_line();
    }

    /* ══════════════════ ALERTS ══════════════════ */
    print_horizontal_line(BOX_LT, BOX_RT);

    if (alerts->count > 0) {
        printf("  %s%s%s  %s%s ALERTS (%d)%s",
               FG_CYAN, BOX_V, RESET, BOLD, FG_RED, alerts->count, RESET);
        int alert_pad = DASH_WIDTH - 4 - 14;
        for (int i = 0; i < alert_pad; i++) printf(" ");
        printf("%s%s%s\n", FG_CYAN, BOX_V, RESET);

        for (int i = 0; i < alerts->count && i < 6; i++) {
            const Alert *a = &alerts->alerts[i];
            const char *icon = (a->level == ALERT_CRITICAL) ? "!!" : " !";
            const char *color = get_alert_color(a->level);

            printf("  %s%s%s  %s%s %s [%-8s] [%-4s]%s %.45s",
                   FG_CYAN, BOX_V, RESET,
                   BOLD, color, icon, get_alert_label(a->level), a->category, RESET,
                   a->message);
            printf("  %s%s%s\n", FG_CYAN, BOX_V, RESET);
        }
    } else {
        printf("  %s%s%s  %s%s SYSTEM STATUS: ALL OK%s",
               FG_CYAN, BOX_V, RESET, BOLD, FG_BGREEN, RESET);
        int ok_pad = DASH_WIDTH - 4 - 24;
        for (int i = 0; i < ok_pad; i++) printf(" ");
        printf("%s%s%s\n", FG_CYAN, BOX_V, RESET);
    }

    /* ══════════════════ FOOTER ══════════════════ */
    print_horizontal_line(BOX_BL, BOX_BR);

    /* Controls hint */
    printf("  %s  Press %sCtrl+C%s%s to exit  |  Refresh: %ds  |  Logs: logs/  |  Reports: reports/%s\n",
           DIM, BOLD, RESET, DIM, REFRESH_INTERVAL, RESET);
    printf("\n");

    fflush(stdout);
}
