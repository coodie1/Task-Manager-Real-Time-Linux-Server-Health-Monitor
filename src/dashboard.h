/*
 * dashboard.h — CLI Dashboard Renderer Interface
 * 
 * Renders a full-screen colored CLI dashboard using ANSI escape codes.
 * Features: progress bars, box drawing, color-coded alerts, system info.
 * OS Concepts: Terminal I/O, escape sequences
 */

#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "shared_data.h"
#include "alerts.h"

/*
 * render_dashboard — Draw the complete dashboard to stdout
 * 
 * Clears the screen and renders all system metrics in a styled
 * box-drawing layout with colored progress bars and alerts.
 *
 * Parameters:
 *   metrics - pointer to the current SystemMetrics data
 *   alerts  - pointer to the current AlertState
 */
void render_dashboard(const SystemMetrics *metrics, const AlertState *alerts);

/*
 * clear_screen — Clear the terminal screen
 */
void clear_screen(void);

/*
 * hide_cursor — Hide the terminal cursor for cleaner display
 */
void hide_cursor(void);

/*
 * show_cursor — Restore the terminal cursor
 */
void show_cursor(void);

#endif /* DASHBOARD_H */
