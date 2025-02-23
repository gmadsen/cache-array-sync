# cache-array-sync

# Photo Sync

A robust photo backup and synchronization tool that maintains versioned backups of your photos while monitoring for changes in real-time.

## Features

- Real-time monitoring and syncing of photo directories
- Versioned backups with timestamps
- Configurable retry mechanism for failed syncs
- Health monitoring and disk space checks
- Log rotation and management
- Lock file mechanism to prevent multiple instances
- Configurable exclusion patterns for unwanted files
- **New:** Configurable health check enable/disable

## Prerequisites

- Linux system with systemd
- rsync
- inotify-tools
- bash 4.0 or higher

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/gmadsen/cache-array-sync.git
   cd photo-sync
   ```

2. Copy the example config:
   ```bash
   sudo cp config/photo-sync.conf.example /etc/photo-sync.conf
   sudo nano /etc/photo-sync.conf
   ```

3. Install Script:
   ```bash
   sudo ./install.sh
   ```

4. Start the service and Enable on boot:
   ```bash
   sudo systemctl start photo-sync
   sudo systemctl enable photo-sync
   ```

5. Command Line Usage:
   ```bash
   photo-sync [-c config_file] {start|stop|status|restart|debug}
   ```

   - Start using default config:
     ```bash
     photo-sync start
     ```

   - Use custom config file:
     ```bash
     photo-sync -c /path/to/custom.conf start
     ```

   - Check service status:
     ```bash
     photo-sync status
     ```

6. Systemd Service:
   - View service status:
     ```bash
     sudo systemctl status photo-sync
     ```

   - View logs:
     ```bash
     sudo journalctl -u photo-sync
     ```

   - Restart service:
     ```bash
     sudo systemctl restart photo-sync
     ```

7. Real-time Monitoring:
   - Monitor the log file in real-time:
     ```bash
     tail -f /var/log/photo-sync.log
     ```

# Usage 
```bash
./photo-sync.sh start    # Start the sync service
./photo-sync.sh stop     # Stop the sync service
./photo-sync.sh status   # Check if it's running
./photo-sync.sh restart  # Restart the service
./photo-sync.sh debug    # Debug mode

## Metrics and Monitoring

Photo Sync includes comprehensive metrics tracking and monitoring capabilities.

### Available Metrics

- `total_syncs`: Total number of sync operations performed
- `failed_syncs`: Number of failed sync operations
- `total_files_synced`: Total number of files synchronized
- `total_bytes_synced`: Total data volume synchronized
- `last_sync_duration`: Duration of the last sync operation

### Prometheus Format Metrics

Metrics are exposed in Prometheus format at `/var/run/photo-sync/metrics/*.prom`:

### Health Monitoring

The service includes comprehensive health checks:
- CPU usage monitoring
- Memory usage tracking
- Disk space trending and prediction
- Sync failure analysis
- System resource monitoring

### Status Reports

# View the latest status report
A detailed status report is generated after each sync operation:
```bash
cat /var/run/photo-sync/metrics/status_report.txt
```

     photo-sync start

# Check sync status
     ```bash
photo-sync status
     ```

# View metrics
     ```bash
cat /var/run/photo-sync/metrics/status_report.txt
     ```

# Monitor disk usage trend
     ```bash
cat /var/run/photo-sync/metrics/disk_usage_history.txt
     ```

# Debugging and Troubleshooting
1. Check sync status:
```bash
photo-sync status
     ```


2. View detailed metrics:
```bash
ls -l /var/run/photo-sync/metrics/
     ```


3. Monitor real-time changes:
    ```bash
tail -f /var/log/photo-sync.log
    ```

