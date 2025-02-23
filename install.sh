#!/bin/bash
# install.sh

# Default paths
INSTALL_DIR="/usr/local/bin"
CONFIG_DIR="/etc"
SERVICE_DIR="/etc/systemd/system"

# Function to prompt for paths
prompt_paths() {
    read -rp "Install directory [$INSTALL_DIR]: " input
    INSTALL_DIR=${input:-$INSTALL_DIR}
    
    read -rp "Config directory [$CONFIG_DIR]: " input
    CONFIG_DIR=${input:-$CONFIG_DIR}
    
    read -rp "Source photos directory: " SOURCE_DIR
    read -rp "Backup destination directory: " DEST_DIR
    read -rp "Versions directory: " VERSIONS_DIR
}

# Function to create directories and set permissions
setup_directories() {
    # Create log directory
    sudo mkdir -p /var/log/photo-sync
    sudo chown "$USER:$USER" /var/log/photo-sync

    # Create run directory
    sudo mkdir -p /var/run/photo-sync
    sudo chown "$USER:$USER" /var/run/photo-sync

    # Create metrics directory
    sudo mkdir -p /var/run/photo-sync/metrics
    sudo chown "$USER:$USER" /var/run/photo-sync/metrics

    # Create backup directories
    mkdir -p "$DEST_DIR"
    mkdir -p "$VERSIONS_DIR"
}

# Function to install config files
install_configs() {
    # Install main config
    if [ ! -f "$CONFIG_DIR/photo-sync.conf" ]; then
        sed -e "s|/path/to/photos|$SOURCE_DIR|" \
            -e "s|/path/to/backup|$DEST_DIR|" \
            -e "s|/path/to/versions|$VERSIONS_DIR|" \
            "config/photo-sync.conf.example" > "$CONFIG_DIR/photo-sync.conf"
    else
        echo "Config file already exists, skipping..."
    fi

    # Install systemd service if it exists
    if [ -f "config/photo-sync.service" ]; then
        sudo cp "config/photo-sync.service" "$SERVICE_DIR/"
        sudo systemctl daemon-reload
    fi
}

# Function to install script
install_script() {
    sudo cp "src/photo-sync.sh" "$INSTALL_DIR/photo-sync"
    sudo chmod +x "$INSTALL_DIR/photo-sync"
}

# Main installation
echo "Photo Sync Installation"
echo "======================"

# Check for root privileges
if [ "$EUID" -ne 0 ]; then 
    echo "Please run with sudo"
    exit 1
fi

# Run installation steps
prompt_paths
setup_directories
install_configs
install_script

echo "Installation complete!"
echo "Edit $CONFIG_DIR/photo-sync.conf to customize settings"