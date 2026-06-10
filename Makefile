
#
# Build System for the Server Health Monitor
#
# Usage:
#   make          — Build the monitor executable
#   make clean    — Remove build artifacts
#   make run      — Build and run the monitor
#   make debug    — Build with debug symbols and run with gdb
#
# OS Concepts: Build automation, compilation, linking

CC       = gcc
CFLAGS   = -Wall -Wextra -Wno-unused-parameter -g -O2
LDFLAGS  =
TARGET   = server_monitor

# Source files
SRC_DIR  = src
SRCS     = $(SRC_DIR)/main.c \
           $(SRC_DIR)/cpu_monitor.c \
           $(SRC_DIR)/mem_monitor.c \
           $(SRC_DIR)/disk_monitor.c \
           $(SRC_DIR)/proc_monitor.c \
           $(SRC_DIR)/dashboard.c \
           $(SRC_DIR)/alerts.c \
           $(SRC_DIR)/logger.c

# Object files
OBJS     = $(SRCS:.c=.o)

# Header files (for dependency tracking)
HDRS     = $(SRC_DIR)/shared_data.h \
           $(SRC_DIR)/cpu_monitor.h \
           $(SRC_DIR)/mem_monitor.h \
           $(SRC_DIR)/disk_monitor.h \
           $(SRC_DIR)/proc_monitor.h \
           $(SRC_DIR)/dashboard.h \
           $(SRC_DIR)/alerts.h \
           $(SRC_DIR)/logger.h

# Default target: build the executable
all: $(TARGET)
	@echo ""
	@echo "  ✓ Build successful: ./$(TARGET)"
	@echo "  Run with: make run  or  ./$(TARGET)"
	@echo ""

# Link object files into final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile .c files to .o files (all depend on headers)
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Run the monitor
run: $(TARGET)
	@echo "Starting Server Health Monitor..."
	@./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "  ✓ Clean complete"

# Debug build
debug: CFLAGS += -DDEBUG -O0
debug: $(TARGET)
	gdb ./$(TARGET)

# Remove stale shared memory (useful if monitor crashes)
clean-shm:
	@ipcrm -M 0x48454C54 2>/dev/null || true
	@echo "  ✓ Shared memory cleaned"

.PHONY: all clean run debug clean-shm
