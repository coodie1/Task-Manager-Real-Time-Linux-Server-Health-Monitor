#!/bin/bash



SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# ── Function: Print styled message ──
print_msg() {
    local color="$1"
    local msg="$2"
    echo -e "${color}${BOLD}  [*] ${msg}${RESET}"
}

# ── Function: Check dependencies ──
check_dependencies() {
    local missing=0
    
    # Check for gcc
    if ! command -v gcc &> /dev/null; then
        print_msg "$RED" "gcc not found. Install with: sudo apt install gcc"
        missing=1
    fi
    
    # Check for make
    if ! command -v make &> /dev/null; then
        print_msg "$RED" "make not found. Install with: sudo apt install make"
        missing=1
    fi
    
    # Check for bc (used in report generator)
    if ! command -v bc &> /dev/null; then
        print_msg "$YELLOW" "bc not found. Install with: sudo apt install bc"
        print_msg "$YELLOW" "Report generation may not work without bc."
    fi
    
    return $missing
}

# ── Main Logic ──
case "${1}" in
    report)
        # Manual report generation
        print_msg "$CYAN" "Generating manual report..."
        bash "$SCRIPT_DIR/report_generator.sh" "$PROJECT_DIR/logs" "$PROJECT_DIR/reports"
        if [ $? -eq 0 ]; then
            print_msg "$GREEN" "Report generated successfully!"
            print_msg "$GREEN" "Check: $PROJECT_DIR/reports/"
        else
            print_msg "$RED" "Report generation failed."
        fi
        ;;
    
    clean)
        # Clean shared memory
        print_msg "$CYAN" "Cleaning stale shared memory..."
        ipcrm -M 0x48454C54 2>/dev/null
        print_msg "$GREEN" "Shared memory cleaned."
        ;;
    
    *)
        # Default: build and run
        echo ""
        echo -e "${CYAN}${BOLD}"
        echo "       Real-Time Linux Server Health Monitor         "
        echo -e "${RESET}"
        
        # Check dependencies
        if ! check_dependencies; then
            print_msg "$RED" "Missing dependencies. Please install them and try again."
            exit 1
        fi
        
        # Clean any stale shared memory from previous runs
        ipcrm -M 0x48454C54 2>/dev/null
        
        # Build
        print_msg "$CYAN" "Compiling project..."
        make -C "$PROJECT_DIR" clean all 2>&1
        
        if [ $? -ne 0 ]; then
            print_msg "$RED" "Build failed! Check errors above."
            exit 1
        fi
        
        print_msg "$GREEN" "Build successful!"
        print_msg "$CYAN" "Starting monitor... (Press Ctrl+C to stop)"
        echo ""
        sleep 1
        
        # Run the monitor
        "$PROJECT_DIR/server_monitor"
        ;;
esac
