/*
 * alerts.h — Alert System Interface
 * 
 * Checks system metrics against configurable thresholds and
 * generates colored alert messages.
 * OS Concepts: Conditional logic, threshold monitoring
 */

#ifndef ALERTS_H
#define ALERTS_H

#include "shared_data.h"

/* Maximum number of active alerts */
#define MAX_ALERTS 16

/* Maximum alert message length */
#define MAX_ALERT_MSG 128

/* Single alert entry */
typedef struct {
    AlertLevel level;
    char       message[MAX_ALERT_MSG];
    char       category[16];   /* "CPU", "MEM", "DISK", "SWAP", "PROC" */
} Alert;

/* Alert state container */
typedef struct {
    Alert alerts[MAX_ALERTS];
    int   count;
} AlertState;

/*
 * check_alerts — Evaluate metrics against thresholds
 * 
 * Populates the AlertState struct with any threshold violations found.
 * Returns the number of active alerts.
 */
int check_alerts(const SystemMetrics *metrics, AlertState *state);

/*
 * get_alert_color — Get ANSI color code for an alert level
 * Returns: "\033[33m" for WARNING (yellow), "\033[31m" for CRITICAL (red)
 */
const char* get_alert_color(AlertLevel level);

/*
 * get_alert_label — Get text label for an alert level
 * Returns: "WARNING" or "CRITICAL"
 */
const char* get_alert_label(AlertLevel level);

#endif /* ALERTS_H */
