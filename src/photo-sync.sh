#!/bin/bash



# Function to load configuration
load_config() {
    # Check if config file exists
    if [ ! -f "$CONFIG_FILE" ]; then
        echo "Warning: Configuration file $CONFIG_FILE not found, using defaults"
        return 1
    fi


    # Source the config file
    # shellcheck source=./config/photo-sync-debug.conf
    # shellcheck disable=SC1091
    if ! source "$CONFIG_FILE"; then
        echo "Error: Failed to load configuration file $CONFIG_FILE"
        exit 1
    fi


    # Validate required settings
    if [ -z "$SOURCE_DIR" ] || [ -z "$DEST_DIR" ]; then
        echo "Error: Required directories not set in configuration"
        exit 1
    fi

    # Validate ENABLE_HEALTH_CHECKS
    if [ "$ENABLE_HEALTH_CHECKS" != "true" ] && [ "$ENABLE_HEALTH_CHECKS" != "false" ]; then
        echo "Error: ENABLE_HEALTH_CHECKS must be true or false"
        exit 1
    fi

    # Create exclude patterns array
    IFS=' ' read -r -a EXCLUDE_ARRAY <<< "$EXCLUDE_PATTERNS"
    RSYNC_EXCLUDE=""
    for pattern in "${EXCLUDE_ARRAY[@]}"; do
        RSYNC_EXCLUDE="$RSYNC_EXCLUDE --exclude='$pattern'"
    done

    # Validate numeric values
    if ! [[ "$HEALTH_CHECK_INTERVAL" =~ ^[0-9]+$ ]]; then
        echo "Error: HEALTH_CHECK_INTERVAL must be a number"
        exit 1
    fi

    if ! [[ "$MIN_DISK_SPACE" =~ ^[0-9]+$ ]]; then
        echo "Error: MIN_DISK_SPACE must be a number"
        exit 1
    fi

    # Create required directories if they don't exist
    mkdir -p "$(dirname "$LOG_FILE")" "$(dirname "$PID_FILE")" "$VERSIONS_DIR"
}

##################### Debugging and Trace #####################
debug_break() {
    if [ "$DEBUG_LEVEL" -ge 3 ]; then
        log_message "DEBUG: Break point hit: $1"
        # This line will trigger breakpoint in bashdb
        echo "Debug break: $1" >&2
    fi
}

trace() {
    if [ "$DEBUG_LEVEL" -ge 3 ]; then
        local func_name="$1"
        shift
        log_message "DEBUG: Entering function: $func_name, args: $*"
        "$func_name" "$@"
        local ret=$?
        log_message "DEBUG: Exiting function: $func_name, return: $ret"
        return $ret
    else
        "$@"
    fi
}

dump_state() {
    if [ "$DEBUG_LEVEL" -ge 3 ]; then
        log_message "DEBUG: Current state dump:"
        log_message "DEBUG: PID: $$"
        log_message "DEBUG: SOURCE_DIR: $SOURCE_DIR"
        log_message "DEBUG: DEST_DIR: $DEST_DIR"
        log_message "DEBUG: Last health check: $LAST_HEALTH_CHECK"
        if [ -f "$PID_FILE" ]; then
            log_message "DEBUG: Main PID: $(cat "$PID_FILE")"
        fi
        if [ -f "$RSYNC_PID_FILE" ]; then
            log_message "DEBUG: Rsync PID: $(cat "$RSYNC_PID_FILE")"
        fi
    fi
}



# Function to acquire lock
acquire_lock() {
    if [ -f "$LOCK_FILE" ]; then
        local pid
        pid=$(cat "$LOCK_FILE" 2>/dev/null)
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            log_message "ERROR: Another instance is already running with PID $pid"
            return 1
        else
            log_message "WARNING: Found stale lock file, removing"
            rm -f "$LOCK_FILE"
        fi
    fi
    echo $$ > "$LOCK_FILE"
    log_message "Lock acquired for PID $$"
    return 0
}

# Function to release lock
release_lock() {
    if [ -f "$LOCK_FILE" ] && [ "$(cat "$LOCK_FILE")" = "$$" ]; then
        rm -f "$LOCK_FILE"
        log_message "Lock released for PID $$"
    fi
}

# Function to rotate log files
rotate_log() {
    if [ -f "$LOG_FILE" ] && [ "$(stat -c%s "$LOG_FILE")" -gt "$MAX_LOG_SIZE" ]; then
        local log_index
        for ((log_index=MAX_LOG_FILES-1; log_index>0; log_index--)); do
            if [ -f "$LOG_FILE.$log_index" ]; then
                mv "$LOG_FILE.$log_index" "$LOG_FILE.$((log_index+1))"
            fi
        done
        mv "$LOG_FILE" "$LOG_FILE.1"
        touch "$LOG_FILE"
    fi
}

# Function to log messages
log_message() {
    local level="INFO"
    local message="$1"
    
    case "$1" in
        "DEBUG:"*)
            [ "$DEBUG_LEVEL" -lt 3 ] && return
            level="DEBUG"
            message="${1#DEBUG: }"
            ;;
        "INFO:"*)
            [ "$DEBUG_LEVEL" -lt 2 ] && return
            level="INFO"
            message="${1#INFO: }"
            ;;
        "WARN:"*)
            [ "$DEBUG_LEVEL" -lt 1 ] && return
            level="WARN"
            message="${1#WARN: }"
            ;;
        "ERROR:"*)
            level="ERROR"
            message="${1#ERROR: }"
            ;;
    esac

    rotate_log
    echo "$(date '+%Y-%m-%d %H:%M:%S') [$level] [$$]: $message" >> "$LOG_FILE"
}



# Function to check disk space
check_disk_space() {
    local available_space
    available_space=$(df -BG "$DEST_DIR" | awk 'NR==2 {print $4}' | sed 's/G//')
    if [ "$available_space" -lt "$MIN_DISK_SPACE" ]; then
        log_message "ERROR: Less than ${MIN_DISK_SPACE}GB space available on destination"
        return 1
    fi
    return 0
}

# Function to check if process is running
check_running() {
    if [ -f "$PID_FILE" ]; then
        if kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
            echo "Photo sync is running (PID: $(cat "$PID_FILE"))"
            return 0
        else
            echo "PID file exists but process is not running"
            rm "$PID_FILE"
            return 1
        fi
    else
        echo "Photo sync is not running"
        return 1
    fi
}

# Function to stop the process
stop_sync() {
    local status=0
    
    if [ -f "$PID_FILE" ]; then
        local OLD_PID
        OLD_PID=$(cat "$PID_FILE")
        if kill -0 "$OLD_PID" 2>/dev/null; then
            log_message "Stopping photo-sync process ($OLD_PID)"
            kill "$OLD_PID"
            sleep 2
            rm -f "$PID_FILE"
        else
            log_message "Process not running but PID file exists. Cleaning up."
            rm -f "$PID_FILE"
            status=1
        fi
    else
        log_message "No PID file found. Process not running."
        status=1
    fi
    
    if [ -f "$RSYNC_PID_FILE" ]; then
        local RSYNC_PID
        RSYNC_PID=$(cat "$RSYNC_PID_FILE")
        if kill -0 "$RSYNC_PID" 2>/dev/null; then
            log_message "Stopping rsync process ($RSYNC_PID)"
            kill "$RSYNC_PID"
            sleep 2
            rm -f "$RSYNC_PID_FILE"
        else
            log_message "Rsync process not running but PID file exists. Cleaning up."
            rm -f "$RSYNC_PID_FILE"
        fi
    fi
    
    release_lock
    return $status
}



# Function to perform sync changes
sync_changes() {
    local retry_count=0
    local success=false

    while [ $retry_count -lt "$MAX_RETRIES" ] && ! $success; do
        log_message "Starting sync operation (attempt $((retry_count + 1)))"
        
        # Build rsync command using configuration
        eval rsync "$RSYNC_OPTS" "$RSYNC_EXCLUDE" \
            --backup \
            --backup-dir="$VERSIONS_DIR/$(date +%Y-%m-%d_%H-%M-%S)" \
            "$SOURCE_DIR/" "$DEST_DIR/" >> "$LOG_FILE" 2>&1 &
        
        echo $! > "$RSYNC_PID_FILE"
        wait $!
        
        if wait $!; then
            success=true
            log_message "Sync completed successfully"
        else
            retry_count=$((retry_count + 1))
            if [ $retry_count -lt "$MAX_RETRIES" ]; then
                log_message "Sync failed, retrying in $RETRY_DELAY seconds"
                sleep "$RETRY_DELAY"
            else
                log_message "Sync failed after $MAX_RETRIES attempts"
                return 1
            fi
        fi
    done
    
    rm -f "$RSYNC_PID_FILE"
    return 0
}

# Enhanced health check function
check_health() {
if [ "$ENABLE_HEALTH_CHECKS" != "true" ]; then
        return 0
    fi

    local current_time
    current_time=$(date +%s)
    if [ $((current_time - LAST_HEALTH_CHECK)) -lt "$HEALTH_CHECK_INTERVAL" ]; then
        return 0
    fi
    LAST_HEALTH_CHECK=$current_time
    local status=0
    if [ -f "$RSYNC_PID_FILE" ]; then
        local rsync_pid
        rsync_pid=$(cat "$RSYNC_PID_FILE")
        if ! pgrep -F "$RSYNC_PID_FILE" >/dev/null; then
            log_message "WARNING: Rsync process ($rsync_pid) died unexpectedly"
            status=1
        fi
    fi
    if [ -f "$LOG_FILE" ]; then
        local last_sync_line
        last_sync_line=$(grep "Sync completed successfully" "$LOG_FILE" | tail -n1)
        if [ -n "$last_sync_line" ]; then
            local last_sync
            last_sync=$(date -d "${last_sync_line%:*}" +%s)
            local time_diff=$((current_time - last_sync))
            if [ $time_diff -gt "$HEALTH_CHECK_INTERVAL" ]; then
                log_message "WARNING: No successful sync for over $((time_diff / 3600)) hours"
                status=1
            fi
        fi
    fi
    if [ ! -r "$SOURCE_DIR" ]; then
        log_message "ERROR: Source directory is not accessible"
        status=1
    fi
    if [ ! -w "$DEST_DIR" ]; then
        log_message "ERROR: Destination directory is not writable"
        status=1
    fi
    return $status
}

# Function to clean up and exit
cleanup_and_exit() {
    log_message "Received shutdown signal"
    stop_sync
    release_lock
    exit 0
}

# Function to start sync
start_sync() {
    # Check for required commands
    if ! command -v inotifywait >/dev/null 2>&1; then
        log_message "ERROR: inotifywait not found. Please install inotify-tools."
        exit 1
    fi
    
    if ! command -v rsync >/dev/null 2>&1; then
        log_message "ERROR: rsync not found."
        exit 1
    fi

    # Try to acquire lock
    if ! acquire_lock; then
        exit 1
    fi


    # Check if source directory exists and is readable
    if [ ! -d "$SOURCE_DIR" ] || [ ! -r "$SOURCE_DIR" ]; then
        echo "Error: Source directory $SOURCE_DIR does not exist or is not readable" >&2
        exit 1
    fi

    # Stop if already running
    if [ -f "$PID_FILE" ]; then
        stop_sync
    fi

    # Ensure directories exist
    mkdir -p "$DEST_DIR" "$VERSIONS_DIR"
    touch "$LOG_FILE"
    
    # Initial health check
    if ! check_health; then
        log_message "WARNING: Initial health check failed"
    fi

    # Check disk space before starting sync
    if ! check_disk_space; then
        log_message "ERROR: Insufficient disk space. Exiting."
        exit 1
    fi

    # Perform initial sync
    log_message "Performing initial sync"
    sync_changes

    # Start monitoring in background
    (inotifywait -m -r -e modify,create,delete,move "$SOURCE_DIR" | while read -r path action file; do
        log_message "$action $path$file"
        sleep "$SYNC_DELAY"
        sync_changes
    done) &

    # Save the PID
    echo $! > "$PID_FILE"
    echo "Photo sync service started with PID $(cat "$PID_FILE"). Monitor log at $LOG_FILE"

    # Ensure cleanup on exit
trap 'cleanup_and_exit' EXIT
}

####################### Main Script #######################

# Default config file location
CONFIG_FILE="$(pwd)/config/photo-sync-debug.conf"

# Initialize LAST_HEALTH_CHECK
LAST_HEALTH_CHECK=0

# Load configuration before starting
trace load_config

# Override config file location from command line
while getopts ":c:" opt; do
    case $opt in
        c)
            CONFIG_FILE="$OPTARG"
            trace load_config
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            exit 1
            ;;
        :)
            echo "Option -$OPTARG requires an argument." >&2
            exit 1
            ;;
    esac
done

trap 'cleanup_and_exit' SIGTERM SIGINT SIGHUP

# Process command line arguments
case "$1" in
    start)
        debug_break "Before starting sync"
        trace start_sync
        debug_break "After starting sync"
        ;;
    stop)
        trace stop_sync
        ;;
    status)
        trace check_running
        ;;
    restart)
        trace stop_sync
        sleep 2
        trace start_sync
        ;;
    debug)
        if [ "$DEBUG_LEVEL" -ge 3 ]; then
            trace dump_state
            log_message "DEBUG: Running in debug mode"
            # Add any specific debug actions here
        else
            echo "Debug mode not enabled. Set DEBUG_LEVEL=3 in config."
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status|restart|debug}"
        exit 1
        ;;
esac
