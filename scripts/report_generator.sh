#!/bin/bash

LOG_DIR="${1:-.logs}"
REPORT_DIR="${2:-./reports}"

# ── Date/Time Variables ──
CURRENT_DATE=$(date +"%Y-%m-%d")
CURRENT_HOUR=$(date +"%H")
DATE_COMPACT=$(date +"%Y%m%d")
TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

# ── File Paths ──
LOG_FILE="${LOG_DIR}/monitor_${DATE_COMPACT}.log"
REPORT_FILE="${REPORT_DIR}/report_${DATE_COMPACT}_${CURRENT_HOUR}.txt"

# ── Create report directory if needed ──
mkdir -p "$REPORT_DIR"

# FUNCTION: calculate_average
calculate_average() {
    local sum=0
    local count=0
    while read -r value; do
        # Remove any non-numeric characters except . and -
        clean_val=$(echo "$value" | sed 's/[^0-9.]//g')
        if [ -n "$clean_val" ]; then
            sum=$(echo "$sum + $clean_val" | bc 2>/dev/null)
            count=$((count + 1))
        fi
    done

    if [ "$count" -gt 0 ]; then
        echo "scale=2; $sum / $count" | bc 2>/dev/null
    else
        echo "0.00"
    fi
}

# ══════════════════════════════════════════
# FUNCTION: find_min_max
# Finds minimum and maximum from a set of numbers
# Arguments: numbers via stdin
# Returns: "min max" on stdout
# ══════════════════════════════════════════
find_min_max() {
    local min=""
    local max=""
    while read -r value; do
        clean_val=$(echo "$value" | sed 's/[^0-9.]//g')
        if [ -z "$clean_val" ]; then continue; fi
        
        if [ -z "$min" ]; then
            min="$clean_val"
            max="$clean_val"
        else
            # Compare using bc for floating point
            if [ "$(echo "$clean_val < $min" | bc 2>/dev/null)" -eq 1 ]; then
                min="$clean_val"
            fi
            if [ "$(echo "$clean_val > $max" | bc 2>/dev/null)" -eq 1 ]; then
                max="$clean_val"
            fi
        fi
    done

    if [ -z "$min" ]; then
        echo "0.00 0.00"
    else
        echo "$min $max"
    fi
}

# ══════════════════════════════════════════
# FUNCTION: count_alerts
# Counts alerts by severity level
# Arguments: $1 = severity (WARNING/CRITICAL)
# Returns: count on stdout
# ══════════════════════════════════════════
count_alerts() {
    local severity="$1"
    local count=0
    if [ -f "$LOG_FILE" ]; then
        count=$(grep -c "^\[.*\] \[$severity\]" "$LOG_FILE" 2>/dev/null)
        # grep -c returns 1 exit code when count is 0, handle it
        if [ -z "$count" ]; then
            count=0
        fi
    fi
    echo "$count"
}

# ══════════════════════════════════════════
# FUNCTION: get_alert_details
# Extracts unique alert messages
# Arguments: $1 = severity
# Returns: unique alert messages on stdout
# ══════════════════════════════════════════
get_alert_details() {
    local severity="$1"
    if [ -f "$LOG_FILE" ]; then
        grep "\[$severity\]" "$LOG_FILE" 2>/dev/null | \
            awk -F']' '{print $NF}' | \
            sort | uniq -c | sort -rn | head -5
    fi
}

# ══════════════════════════════════════════
# FUNCTION: extract_metric
# Extracts a specific metric value from log entries
# Arguments: $1 = metric name (e.g., "CPU")
# Returns: metric values on stdout (one per line)
# ══════════════════════════════════════════
extract_metric() {
    local metric_name="$1"
    if [ -f "$LOG_FILE" ]; then
        grep "\[METRIC\]" "$LOG_FILE" 2>/dev/null | \
            grep -oP "${metric_name}:[0-9.]+" | \
            sed "s/${metric_name}://"
    fi
}

# ══════════════════════════════════════════
# MAIN REPORT GENERATION
# ══════════════════════════════════════════

# Check if log file exists
if [ ! -f "$LOG_FILE" ]; then
    echo "No log file found at: $LOG_FILE"
    echo "Report cannot be generated without log data."
    exit 1
fi

# ── Extract Metrics into Arrays ──
# Using arrays (Lab 13 concept)
declare -a cpu_values
declare -a mem_values
declare -a disk_values
declare -a swap_values

# Read CPU values into array
mapfile -t cpu_values < <(extract_metric "CPU")
mapfile -t mem_values < <(extract_metric "MEM")
mapfile -t disk_values < <(extract_metric "DISK")
mapfile -t swap_values < <(extract_metric "SWAP")

# ── Calculate Statistics ──
cpu_avg=$(printf '%s\n' "${cpu_values[@]}" | calculate_average)
cpu_minmax=$(printf '%s\n' "${cpu_values[@]}" | find_min_max)
cpu_min=$(echo "$cpu_minmax" | awk '{print $1}')
cpu_max=$(echo "$cpu_minmax" | awk '{print $2}')

mem_avg=$(printf '%s\n' "${mem_values[@]}" | calculate_average)
mem_minmax=$(printf '%s\n' "${mem_values[@]}" | find_min_max)
mem_min=$(echo "$mem_minmax" | awk '{print $1}')
mem_max=$(echo "$mem_minmax" | awk '{print $2}')

disk_avg=$(printf '%s\n' "${disk_values[@]}" | calculate_average)
disk_minmax=$(printf '%s\n' "${disk_values[@]}" | find_min_max)
disk_min=$(echo "$disk_minmax" | awk '{print $1}')
disk_max=$(echo "$disk_minmax" | awk '{print $2}')

swap_avg=$(printf '%s\n' "${swap_values[@]}" | calculate_average)

# ── Count Entries and Alerts ──
total_entries=$(grep -c "\[METRIC\]" "$LOG_FILE" 2>/dev/null || echo 0)
warning_count=$(count_alerts "WARNING")
critical_count=$(count_alerts "CRITICAL")

# ── Get Monitoring Duration ──
first_time=$(grep "\[METRIC\]" "$LOG_FILE" 2>/dev/null | head -1 | grep -oP '\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}')
last_time=$(grep "\[METRIC\]" "$LOG_FILE" 2>/dev/null | tail -1 | grep -oP '\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}')

# ══════════════════════════════════════════
# WRITE REPORT FILE
# Using redirection (Lab 4 concept)
# ══════════════════════════════════════════
{
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║           SERVER HEALTH MONITOR - SUMMARY REPORT           ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo ""
    echo "  Report Generated: ${TIMESTAMP}"
    echo "  Report Period:    ${first_time:-N/A} to ${last_time:-N/A}"
    echo "  Total Samples:    ${total_entries}"
    echo "  Log File:         ${LOG_FILE}"
    echo ""
    echo "══════════════════════════════════════════════════════════════"
    echo "  RESOURCE USAGE SUMMARY"
    echo "══════════════════════════════════════════════════════════════"
    echo ""
    echo "  ┌─────────────┬──────────┬──────────┬──────────┐"
    echo "  │   Metric    │ Average  │ Minimum  │ Maximum  │"
    echo "  ├─────────────┼──────────┼──────────┼──────────┤"
    printf "  │ %-11s │ %6s%%  │ %6s%%  │ %6s%%  │\n" "CPU Usage" "$cpu_avg" "$cpu_min" "$cpu_max"
    printf "  │ %-11s │ %6s%%  │ %6s%%  │ %6s%%  │\n" "Memory" "$mem_avg" "$mem_min" "$mem_max"
    printf "  │ %-11s │ %6s%%  │ %6s%%  │ %6s%%  │\n" "Disk" "$disk_avg" "$disk_min" "$disk_max"
    printf "  │ %-11s │ %6s%%  │   N/A    │   N/A    │\n" "Swap" "$swap_avg"
    echo "  └─────────────┴──────────┴──────────┴──────────┘"
    echo ""
    echo "══════════════════════════════════════════════════════════════"
    echo "  ALERT SUMMARY"
    echo "══════════════════════════════════════════════════════════════"
    echo ""
    echo "  Total Warnings:  ${warning_count}"
    echo "  Total Critical:  ${critical_count}"
    echo ""

    if [ "$warning_count" -gt 0 ] || [ "$critical_count" -gt 0 ]; then
        echo "  Top Warning Events:"
        get_alert_details "WARNING" | while read -r line; do
            echo "    ${line}"
        done
        echo ""
        echo "  Top Critical Events:"
        get_alert_details "CRITICAL" | while read -r line; do
            echo "    ${line}"
        done
        echo ""
    else
        echo "  No alerts triggered during this period."
        echo ""
    fi

    echo "══════════════════════════════════════════════════════════════"
    echo "  SYSTEM HEALTH ASSESSMENT"
    echo "══════════════════════════════════════════════════════════════"
    echo ""

    # Health assessment logic using conditionals (Lab 12 concept)
    if [ "$critical_count" -gt 0 ]; then
        echo "  Status: !! CRITICAL - Immediate attention required"
        echo "  ${critical_count} critical event(s) detected during monitoring period."
    elif [ "$warning_count" -gt 5 ]; then
        echo "  Status: ! WARNING - System experiencing elevated resource usage"
        echo "  ${warning_count} warning event(s) suggest resource pressure."
    elif [ "$warning_count" -gt 0 ]; then
        echo "  Status: CAUTION - Minor resource warnings detected"
        echo "  ${warning_count} warning(s) detected but system is generally healthy."
    else
        echo "  Status: HEALTHY - All systems operating within normal parameters"
        echo "  No alerts triggered during the monitoring period."
    fi

    echo ""
    echo "══════════════════════════════════════════════════════════════"
    echo "  END OF REPORT"
    echo "══════════════════════════════════════════════════════════════"

} > "$REPORT_FILE"

echo "Report generated: $REPORT_FILE"
exit 0
