/*
 * disk_monitor.c — Disk Monitoring Implementation
 * 
 * Uses statvfs() system call for disk space metrics on the root filesystem.
 * Reads /proc/diskstats to calculate disk I/O throughput (read/write KB/s).
 *
 * OS Concepts: /proc filesystem (Lab 10-11), System calls, File I/O (Lab 4)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include "disk_monitor.h"

/*
 * Structure to hold raw sector counts from /proc/diskstats
 */
typedef struct {
    unsigned long long sectors_read;
    unsigned long long sectors_written;
} DiskIO;

/*
 * read_disk_io — Parse /proc/diskstats for the primary disk device
 *
 * /proc/diskstats format (field numbers):
 *   major minor name reads_completed reads_merged sectors_read time_reading
 *   writes_completed writes_merged sectors_written time_writing ...
 *
 * We look for 'sda' or 'nvme0n1' or 'vda' as the primary disk.
 * Each sector is 512 bytes.
 */
static int read_disk_io(DiskIO *dio)
{
    FILE *fp = fopen("/proc/diskstats", "r");
    if (!fp) return -1;

    char line[512];
    unsigned int major, minor;
    char devname[64];
    unsigned long long rd_ios, rd_merges, rd_sectors, rd_ticks;
    unsigned long long wr_ios, wr_merges, wr_sectors, wr_ticks;
    int found = 0;

    /* Target device names to look for (in priority order) */
    const char *targets[] = {"sda", "nvme0n1", "vda", "xvda", "mmcblk0", NULL};

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu",
                   &major, &minor, devname,
                   &rd_ios, &rd_merges, &rd_sectors, &rd_ticks,
                   &wr_ios, &wr_merges, &wr_sectors, &wr_ticks) >= 11) {
            for (int i = 0; targets[i]; i++) {
                if (strcmp(devname, targets[i]) == 0) {
                    dio->sectors_read = rd_sectors;
                    dio->sectors_written = wr_sectors;
                    found = 1;
                    break;
                }
            }
            if (found) break;
        }
    }
    fclose(fp);

    /* If no specific device found, try to sum all non-partition devices */
    if (!found) {
        dio->sectors_read = 0;
        dio->sectors_written = 0;
        fp = fopen("/proc/diskstats", "r");
        if (!fp) return -1;
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "%u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu",
                       &major, &minor, devname,
                       &rd_ios, &rd_merges, &rd_sectors, &rd_ticks,
                       &wr_ios, &wr_merges, &wr_sectors, &wr_ticks) >= 11) {
                /* Use the first device with significant I/O */
                if (rd_sectors > 0 || wr_sectors > 0) {
                    dio->sectors_read = rd_sectors;
                    dio->sectors_written = wr_sectors;
                    found = 1;
                    break;
                }
            }
        }
        fclose(fp);
    }

    return found ? 0 : -1;
}

/*
 * collect_disk_metrics — Main disk data collection function
 *
 * Uses statvfs() for disk space on root filesystem "/".
 * Uses /proc/diskstats with the delta method for I/O speed.
 */
void collect_disk_metrics(SystemMetrics *metrics)
{
    /* ── Disk space using statvfs() ── */
    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0) {
        unsigned long long total_bytes = (unsigned long long)vfs.f_blocks * vfs.f_frsize;
        unsigned long long free_bytes  = (unsigned long long)vfs.f_bfree  * vfs.f_frsize;
        unsigned long long avail_bytes = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
        unsigned long long used_bytes  = total_bytes - free_bytes;

        metrics->disk_total_mb = (long)(total_bytes / (1024 * 1024));
        metrics->disk_free_mb  = (long)(avail_bytes / (1024 * 1024));
        metrics->disk_used_mb  = (long)(used_bytes  / (1024 * 1024));

        if (total_bytes > 0) {
            metrics->disk_usage_percent = (float)used_bytes / total_bytes * 100.0;
        } else {
            metrics->disk_usage_percent = 0.0;
        }
    }

    /* ── Disk I/O speed using /proc/diskstats delta method ── */
    DiskIO dio1, dio2;
    if (read_disk_io(&dio1) == 0) {
        usleep(500000);  /* Wait 500ms for delta measurement */
        if (read_disk_io(&dio2) == 0) {
            /* Each sector = 512 bytes, measured over 0.5s, convert to KB/s */
            long long rd_delta = dio2.sectors_read - dio1.sectors_read;
            long long wr_delta = dio2.sectors_written - dio1.sectors_written;
            if (rd_delta < 0) rd_delta = 0;
            if (wr_delta < 0) wr_delta = 0;

            /* sectors * 512 bytes / 1024 = KB, then *2 because 0.5s interval */
            metrics->disk_read_speed_kb  = (long)(rd_delta * 512 / 1024 * 2);
            metrics->disk_write_speed_kb = (long)(wr_delta * 512 / 1024 * 2);
        }
    }

    /* Signal that disk data is ready */
    metrics->disk_ready = 1;
}
