#!/bin/bash

# Serial controller for LED Strip Games
# Controls games via serial on Arduino

SERIAL_PORT="/dev/ttyACM0"
BAUD_RATE=9600

# Save terminal settings
old_stty=""
serial_connected=1
drain_pid=""

# Cleanup function
cleanup() {
    # Kill the drain process if running
    if [ -n "$drain_pid" ] && kill -0 "$drain_pid" 2>/dev/null; then
        kill "$drain_pid" 2>/dev/null
        wait "$drain_pid" 2>/dev/null
    fi
    
    # Close serial port file descriptors
    exec 3>&- 2>/dev/null
    exec 4<&- 2>/dev/null
    
    # Restore terminal settings
    if [ -n "$old_stty" ]; then
        stty "$old_stty" 2>/dev/null
    fi
    
    echo ""
    echo "Goodbye!"
    exit 0
}

# Trap ctrl+c and call cleanup
trap cleanup INT EXIT

# Function to drain serial input in background
start_drain() {
    # Read and discard all incoming serial data to prevent Arduino TX buffer backup
    while true; do
        cat <&4 >/dev/null 2>&1
    done &
    drain_pid=$!
}

# Function to open/reconnect serial port
open_serial() {
    # Kill existing drain process
    if [ -n "$drain_pid" ] && kill -0 "$drain_pid" 2>/dev/null; then
        kill "$drain_pid" 2>/dev/null
        wait "$drain_pid" 2>/dev/null
    fi
    
    # Close existing connections if open
    exec 3>&- 2>/dev/null
    exec 4<&- 2>/dev/null
    
    echo ""
    echo "Connecting to $SERIAL_PORT..."
    
    # Check if serial port exists
    if [ ! -e "$SERIAL_PORT" ]; then
        echo "Error: $SERIAL_PORT not found!"
        echo "Make sure your Arduino is connected."
        echo "Press [R] to retry or [Q] to quit."
        serial_connected=1
        return 1
    fi
    
    # Configure serial port with raw mode and no flow control
    stty -F "$SERIAL_PORT" $BAUD_RATE cs8 -cstopb -parenb raw -echo -ixon -ixoff 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "Error: Failed to configure $SERIAL_PORT"
        echo "Press [R] to retry or [Q] to quit."
        serial_connected=1
        return 1
    fi
    
    # Open serial port for writing (fd 3) and reading (fd 4)
    exec 3>"$SERIAL_PORT"
    exec 4<"$SERIAL_PORT"
    
    if [ $? -ne 0 ]; then
        echo "Error: Failed to open $SERIAL_PORT"
        echo "Press [R] to retry or [Q] to quit."
        serial_connected=1
        return 1
    fi
    
    # Start draining Arduino output in background
    start_drain
    
    echo "Connected to $SERIAL_PORT at $BAUD_RATE baud"
    echo ""
    serial_connected=0
    return 0
}

print_help() {
    echo "========================================="
    echo "     LED STRIP GAMES - Serial Controller"
    echo "========================================="
    echo ""
    echo "Game Selection (from menu):"
    echo "  [W] - Color Shooter"
    echo "  [R] - Tug of War"
    echo "  [B] - Reaction Zone"
    echo "  [Y] - Pacman 1D"
    echo "  [0] - Back to Menu (during game)"
    echo ""
    echo "Pacman Controls:"
    echo "  [W] [R] - Move LEFT"
    echo "  [B] [Y] [G] - Move RIGHT"
    echo ""
    echo "System:"
    echo "  [Shift+R] - Reconnect Serial"
    echo "  [Shift+H] - Show this help"
    echo "  [Shift+Q] - Quit"
    echo ""
    echo "========================================="
}

send_char() {
    if [ $serial_connected -eq 0 ]; then
        printf '%s' "$1" >&3 2>/dev/null
    fi
}

# Print help
print_help

# Save terminal settings
old_stty=$(stty -g)

# Initial serial connection
open_serial

# Set terminal to raw mode with immediate input (no buffering)
stty raw -echo min 0 time 1

# Main loop - read single characters using bash builtin
while true; do
    # Read a single character
    # -n 1: read exactly 1 character
    # -s: silent (don't echo)
    # -t 0.02: small timeout to keep loop responsive
    if IFS= read -rsn1 -t 0.02 char && [ -n "$char" ]; then
        case "$char" in
            # Back to menu
            0)
                send_char "0"
                ;;
            # Color buttons - game selection AND in-game controls
            w|r|b|y|g)
                send_char "$char"
                ;;
            # Cycle colors (Color Shooter)
            d)
                send_char "d"
                ;;
            # System commands (Shift + key = uppercase)
            R)
                stty "$old_stty"
                open_serial
                stty raw -echo min 0 time 1
                ;;
            H)
                stty "$old_stty"
                print_help
                stty raw -echo min 0 time 1
                ;;
            Q)
                cleanup
                ;;
        esac
    fi
done
