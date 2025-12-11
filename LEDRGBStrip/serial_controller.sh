#!/bin/bash

# Serial controller for Color Shooter Game
# Sends 'a' (shoot) and 'd' (change color) to Arduino

SERIAL_PORT="/dev/ttyACM0"
BAUD_RATE=9600
SERIAL_FD=3

# Save terminal settings
old_stty=""

# Cleanup function
cleanup() {
    # Close serial port file descriptor
    exec 3>&- 2>/dev/null
    
    # Restore terminal settings
    if [ -n "$old_stty" ]; then
        stty "$old_stty"
    fi
    
    echo ""
    echo "Goodbye!"
    exit 0
}

# Trap ctrl+c and call cleanup
trap cleanup INT EXIT

# Function to open/reconnect serial port
open_serial() {
    # Close existing connection if open
    exec 3>&- 2>/dev/null
    
    echo ""
    echo "Connecting to $SERIAL_PORT..."
    
    # Check if serial port exists
    if [ ! -e "$SERIAL_PORT" ]; then
        echo "Error: $SERIAL_PORT not found!"
        echo "Make sure your Arduino is connected."
        echo "Press [R] to retry or [Q] to quit."
        return 1
    fi
    
    # Configure serial port
    stty -F "$SERIAL_PORT" $BAUD_RATE cs8 -cstopb -parenb -echo 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "Error: Failed to configure $SERIAL_PORT"
        echo "Press [R] to retry or [Q] to quit."
        return 1
    fi
    
    # Open serial port on file descriptor 3 for writing
    exec 3>"$SERIAL_PORT"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to open $SERIAL_PORT"
        echo "Press [R] to retry or [Q] to quit."
        return 1
    fi
    
    echo "Connected to $SERIAL_PORT at $BAUD_RATE baud"
    echo ""
    return 0
}

print_help() {
    echo "=== Color Shooter Serial Controller ==="
    echo ""
    echo "Controls:"
    echo "  [A] - Shoot"
    echo "  [D] - Change Color"
    echo "  [R] - Reconnect Serial"
    echo "  [Q] - Quit"
    echo ""
    echo "Press keys to play!"
    echo "========================================"
}

# Print help
print_help

# Save terminal settings
old_stty=$(stty -g)

# Initial serial connection
open_serial
serial_connected=$?

# Set terminal to raw mode (no need to press Enter)
stty raw -echo

# Main loop - read single characters
while true; do
    # Read a single character
    char=$(dd bs=1 count=1 2>/dev/null)
    
    case "$char" in
        a|A)
            if [ $serial_connected -eq 0 ]; then
                echo -n "a" >&3 2>/dev/null
            fi
            ;;
        d|D)
            if [ $serial_connected -eq 0 ]; then
                echo -n "d" >&3 2>/dev/null
            fi
            ;;
        r|R)
            # Temporarily restore terminal for output
            stty "$old_stty"
            open_serial
            serial_connected=$?
            stty raw -echo
            ;;
        q|Q)
            cleanup
            ;;
    esac
done
