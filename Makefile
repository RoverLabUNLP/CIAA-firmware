.DEFAULT_GOAL := build
CIAA_SDK_DIR ?= $(HOME)/CIAA_Software_1.1-linux-x64
CIAA_FIRMWARE := $(CIAA_SDK_DIR)/ide4plc/shared/firmware_v3
export PATH := $(CIAA_SDK_DIR)/tools/gcc-arm-embedded/bin:$(PATH)

.PHONY: build clean configure
configure:
	bash tools/configure.sh "$(CIAA_SDK_DIR)"

build: configure
	$(MAKE) -f "$(CIAA_FIRMWARE)/Makefile" PROGRAM_NAME=app BOARD=edu_ciaa_nxp all

clean: configure
	$(MAKE) -f "$(CIAA_FIRMWARE)/Makefile" PROGRAM_NAME=app BOARD=edu_ciaa_nxp clean
