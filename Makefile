# --- START OF CONFIG ---------------------------------------------------
# Edit the following variables for your own needs

# toolchain configuration
PREFIX  ?= avr-
CC       = $(PREFIX)gcc
LD       = $(PREFIX)gcc
OBJCOPY  = $(PREFIX)objcopy
OBJDUMP  = $(PREFIX)objdump
GDB      = $(PREFIX)gdb
SIZE     = $(PREFIX)size

MCU := atmega328p
SERIAL_PORT ?= /dev/ttyUSB0

AVRDUDE = avrdude
AVRDUDE_FLAGS = -p $(MCU) -c arduino -b 57600 -P $(SERIAL_PORT)

# default build configuration
# "make BUILD=release" does a release build
BUILD:=debug

# basic build flags configuration
CFLAGS+=-Wall -std=c11 -pedantic -Wextra -Wimplicit-function-declaration \
        -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes \
        -Wundef -Wshadow -Werror

LDFLAGS+=

# hardware config
CFLAGS += -DF_CPU=16000000UL -mmcu=$(MCU)
LDFLAGS += -mmcu=$(MCU)

# build type specific flags
CFLAGS_debug=-O0 -ggdb -DDEBUG
LDFLAGS_debug=

CFLAGS_release=-O2 -ggdb
LDFLAGS_release=

# target configuration
TARGET := rollerlicht
VERSION := 0.0.0
VCSVERSION := $(shell git rev-parse --short HEAD)

# source files for the project
SOURCE := $(shell find src/ -name '*.c')
INCLUDES := $(shell find src/ -name '*.h')

# additional dependencies for build (proper targets must be specified by user)
DEPS := 

# default target
all: $(TARGET)

# --- END OF CONFIG -----------------------------------------------------

OBJ1=$(patsubst %.c, %.o, $(SOURCE))
OBJ=$(patsubst src/%, obj/$(BUILD)/%, $(OBJ1))

VERSIONSTR="\"$(VERSION)-$(VCSVERSION)\""

CFLAGS+=-DVERSION=$(VERSIONSTR)

TARGET_BASE := bin/$(BUILD)/$(TARGET)

CFLAGS+=$(CFLAGS_$(BUILD))
LDFLAGS+=$(LDFLAGS_$(BUILD))

.PHONY show_cflags:
	@echo --- Build parameters:  ------------------------------------------
	@echo CFLAGS\=$(CFLAGS)
	@echo LDFLAGS\=$(LDFLAGS)
	@echo SOURCE\=$(SOURCE)
	@echo -----------------------------------------------------------------

$(TARGET): show_cflags $(TARGET_BASE).elf $(TARGET_BASE).hex \
	         $(TARGET_BASE).lss $(TARGET_BASE).bin
	@$(SIZE) $(TARGET_BASE).elf
	@echo ">>> $(BUILD) build complete."

$(TARGET_BASE).elf: $(DEPS) $(OBJ) $(INCLUDES) Makefile
	@echo Linking $@ ...
	@mkdir -p $(shell dirname $@)
	@$(LD) -o $(TARGET_BASE).elf $(OBJ) $(LDFLAGS)

$(TARGET_BASE).hex: $(TARGET_BASE).elf
	@echo "Generating $@ ..."
	@$(OBJCOPY) -Oihex $< $@

$(TARGET_BASE).bin: $(TARGET_BASE).elf
	@echo "Generating $@ ..."
	@$(OBJCOPY) -Obinary $< $@

$(TARGET_BASE).lss: $(TARGET_BASE).elf
	@echo "Generating $@ ..."
	@$(OBJDUMP) -S $< > $@

obj/$(BUILD)/%.o: src/%.c $(INCLUDES) Makefile
	@echo "Compiling $< ..."
	@mkdir -p $(shell dirname $@)
	@$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET_BASE).elf
	rm -f $(TARGET_BASE).hex
	rm -f $(TARGET_BASE).lss
	rm -f $(TARGET_BASE).bin
	rm -f $(OBJ)

program: program_avrdude

reset: reset_avrdude

program_avrdude: $(TARGET_BASE).hex
	@echo "AVRDUDE $@ ..."
	@$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(TARGET_BASE).hex
