#This makefile is derived from a pretty old project and there is lots of errors and bad practices in it
#TODO: Rewrite by utilizing one of the newer ones for template

#Project settings
PROJECT_NAME = led_böp
SOURCES = test_led_driver_on_flash.c led_driver.c uart_driver.c
BUILD_DIR = build/

PORT ?= /dev/ttyUSB0
STM32_FLASH_SETTINGS ?= -B cs.dtr -R cs.rts -b 1000000

OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)%.o)
MACRO_DEFS = $(SOURCES:%.c=$(BUILD_DIR)%.macro_defs)
TARGET_ELF = $(BUILD_DIR)$(PROJECT_NAME).elf
TARGET_BIN = $(TARGET_ELF:%.elf=%.bin)

#Toolchain settings
TOOLCHAIN = arm-none-eabi

CC = $(TOOLCHAIN)-gcc
OBJCOPY = $(TOOLCHAIN)-objcopy
OBJDUMP = $(TOOLCHAIN)-objdump
SIZE = $(TOOLCHAIN)-size

#Target CPU options
CPU_DEFINES = -mthumb -mcpu=cortex-m3 -msoft-float -DSTM32F1
DEVICE = stm32f100c6


default: $(TARGET_BIN)

#Configure ld file
OPENCM3_DIR = lib/libopencm3
include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

#LINK_SCRIPT = stm32f100x6-ram.ld

LINK_SCRIPT = $(LDSCRIPT)



#Compiler options
CFLAGS += -g -c -std=gnu99 -O4 -Wall -fno-common -ffunction-sections
CFLAGS += -fdata-sections
CFLAGS += $(CPU_DEFINES)

INCLUDE_PATHS += -Ilib/libopencm3/include -Iinc



LINK_FLAGS =  -Llib/libopencm3/lib 
LINK_FLAGS += -Llib/libopencm3/lib/stm32/f1 
LINK_FLAGS += -T$(LINK_SCRIPT) -lopencm3_stm32f1 #-lc
LINK_FLAGS += -Wl,--gc-sections -nostartfiles #-nodefaultlibs #-nostdlib

LIBS = libopencm3_stm32f1.a

#Not used for now but we should add it
DEBUG_FLAGS = -g   



#Directories
vpath %.c src
vpath %.o $(BUILD_DIR)
vpath %.ld lib/libopencm3/lib/stm32/f1
vpath %.a lib/libopencm3/lib


testing: $(TEST_ELF)
	./$(TEST_ELF)



$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $(TARGET_ELF) $(TARGET_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET_ELF): $(BUILD_DIR) $(LIBS) $(OBJECTS) $(LINK_SCRIPT)
	$(CC) $(OBJECTS) $(LINK_FLAGS) -o $(TARGET_ELF)

$(TEST_ELF): $(BUILD_DIR) $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(TEST_LINK_FLAGS) -o $(TEST_ELF)


$(OBJECTS): $(BUILD_DIR)%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $^ -o $@

$(MACRO_DEFS): $(BUILD_DIR)%.macro_defs: %.c
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $^ -fdump-final-insns=$@

$(LINK_SCRIPT): libopencm3_stm32f1.a

libopencm3_stm32f1.a: lib/libopencm3/.git
	cd lib/libopencm3; $(MAKE) lib/stm32/f1

lib/libopencm3/.git:
	cd lib/libopencm3; git submodule init
	cd lib/libopencm3; git submodule update

clean:
	rm -f $(OBJECTS) $(TARGET_ELF) $(TARGET_BIN)

deep-clean: clean
	cd lib/libopencm3; $(MAKE) clean

upload: $(TARGET_BIN)
	stm32flash $(PORT) $(STM32_FLASH_SETTINGS) -w $(TARGET_BIN)

.PHONY: default clean deep-clean libopencm3 upload testing

#makefile_debug:
