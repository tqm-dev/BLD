# ==========================================
# Beacon Light Daemon
# ==========================================

# Compiler and compilation flags
CC       := gcc
CFLAGS   := -Wall -Wextra -O2 -g -I. -Iccan -pthread
LDFLAGS  := -pthread

# Target executable name
TARGET   := beaconlited

# 1. Main application source files
SRCS     := main.c

# 2. Collect CCAN source files
CCAN_DIR  := ccan/ccan
CCAN_SRCS := $(CCAN_DIR)/io/io.c \
             $(CCAN_DIR)/io/poll.c \
             $(CCAN_DIR)/tal/tal.c \
             $(CCAN_DIR)/tal/str/str.c \
             $(CCAN_DIR)/take/take.c \
             $(CCAN_DIR)/ilog/ilog.c \
             $(CCAN_DIR)/time/time.c \
             $(CCAN_DIR)/timer/timer.c \
             $(CCAN_DIR)/list/list.c
SRCS      += $(CCAN_SRCS)

# Convert source list (.c) to object files (.o)
OBJS     := $(SRCS:.c=.o)

# ------------------------------------------
# Build Rules
# ------------------------------------------

.PHONY: all clean run

# Default target
all: config.h $(TARGET)

# Link object files to create the final executable
$(TARGET): $(OBJS)
	@echo "Linking $@..."
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build successful! Run with: ./$(TARGET)"

# Compile each source file (.c) into an object file (.o)
%.o: %.c config.h
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Automatically generate config.h using the correct nested path
config.h:
	@echo "Generating config.h using CCAN configurator..."
	$(CC) -o ccan/tools/configurator/configurator ccan/tools/configurator/configurator.c
	./ccan/tools/configurator/configurator $(CC) > config.h

# Clean up generated object files and executable
clean:
	@echo "Cleaning up..."
	rm -f $(TARGET) config.h ccan/tools/configurator/configurator
	find ccan -name "*.o" -delete 2>/dev/null
	rm -f *.o

# Helper command to build and immediately run the node
run: all
	./$(TARGET)
