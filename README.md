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


2. Copy the example config
sudo cp config/photo-sync.conf.example /etc/photo-sync.conf
sudo nano /etc/photo-sync.conf

3. Install Script
sudo ./install.sh

4. Start the service and Enable on boot
sudo systemctl start photo-sync
sudo systemctl enable photo-sync


5. Command Line Usage
photo-sync [-c config_file] {start|stop|status|restart}

# Start using default config
photo-sync start

# Use custom config file
photo-sync -c /path/to/custom.conf start

# Check service status
photo-sync status



6. Systemd Service
# View service status
sudo systemctl status photo-sync

# View logs
sudo journalctl -u photo-sync

# Restart service
sudo systemctl restart photo-sync

7. Real-time Monitoring
# Monitor the log file in real-time
tail -f /var/log/photo-sync.log



# Usage 
# ./photo-sync.sh start    # Start the sync service
# ./photo-sync.sh stop     # Stop the sync service
# ./photo-sync.sh status   # Check if it's running
# ./photo-sync.sh restart  # Restart the service
# ./photo-sync.sh debug    # Debug mode
