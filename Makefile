# Genumi - chatbot NumWorks (.nwa). Base : NumMasterGear (Makefile adapté).
#
#   make clean && make            ->  output/Genumi.nwa

Q ?= @

CC = arm-none-eabi-gcc
BUILD_DIR = output

NWPRE = $(HOME)/nwpre.js
NWLINK = node -r $(NWPRE) $(HOME)/nwtools/node_modules/nwlink/bin/nwlink

src = src/main.c src/meta.c src/suggest.c src/save.c src/kb.c src/engine.c src/dict.c

EADK_CFLAGS := $(shell $(NWLINK) eadk-cflags)

CFLAGS = -std=gnu99
CFLAGS += $(EADK_CFLAGS)
CFLAGS += -O2 -Wall
CFLAGS += -DPLATFORM_DEVICE=1 -Isrc -Isrc/common
CFLAGS += -DNW_APP_NAME=\"Genumi\"
CFLAGS += -fdata-sections -ffunction-sections

LDFLAGS = -Wl,--relocatable
LDFLAGS += -nostartfiles
LDFLAGS += --specs=nano.specs
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level
LDFLAGS += -Wl,--gc-sections

OBJ = $(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(src)))

.PHONY: build clean
build: $(BUILD_DIR)/Genumi.nwa

$(BUILD_DIR)/Genumi.nwa: $(OBJ) $(BUILD_DIR)/icon.o
	@echo "LD  $@"
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	$(Q) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/icon.o: genumi_flat.png
	@mkdir -p $(BUILD_DIR)
	@echo "ICON $<"
	$(Q) $(NWLINK) png-icon-o $< $@
	$(Q) NAME=$$(arm-none-eabi-readelf -SW $@ | grep -o '\.rodata\.eadk[^ ]*' | head -1); \
	arm-none-eabi-objcopy --set-section-flags "$$NAME"=alloc,load,readonly,data $@ $@.ro && mv $@.ro $@

clean:
	@echo "CLEAN"
	$(Q) rm -rf $(BUILD_DIR)
