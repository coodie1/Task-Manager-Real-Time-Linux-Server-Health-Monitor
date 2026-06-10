/*
 * logger.h — Timestamped Event Logging Interface
 * 
 * Provides functions to write timestamped log entries to daily log files.
 * OS Concepts: File I/O, text processing, timestamps
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "shared_data.h"
#include "alerts.h"

/* Log level identifiers */
typedef enum {
    LOG_INFO,
    LOG_WARNING,
    LOG_CRITICAL,
    LOG_ERROR
} LogLevel;

/*
 * logger_init — Initialize the logging system
 * Creates the logs directory if it doesn't exist.
 * Opens (or creates) today's log file.
 *
 * Parameters:
 *   log_dir - path to the logs directory
 *
 * Returns 0 on success, -1 on failure.
 */
int logger_init(const char *log_dir);

/*
 * logger_close — Close the current log file
 */
void logger_close(void);

/*
 * log_metrics — Write a metrics snapshot to the log file
 * 
 * Format: [YYYY-MM-DD HH:MM:SS] [INFO] CPU: XX.X% | MEM: XX.X% | DISK: XX.X% | SWAP: XX.X%
 */
void log_metrics(const SystemMetrics *metrics);

/*
 * log_alerts — Write alert entries to the log file
 *
 * Format: [YYYY-MM-DD HH:MM:SS] [WARNING/CRITICAL] Alert message
 */
void log_alerts(const AlertState *alerts);

/*
 * log_message — Write a generic timestamped message
 */
void log_message(LogLevel level, const char *message);

/*
 * get_log_filepath — Get the path to today's log file
 * Returns pointer to static buffer containing the path.
 */
const char* get_log_filepath(void);

#endif /* LOGGER_H */
