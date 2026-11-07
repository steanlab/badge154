OBJS += \
$(OUT_PATH)/app.o \
$(OUT_PATH)/app_att.o \
$(OUT_PATH)/battery.o \
$(OUT_PATH)/ble.o \
$(OUT_PATH)/epd_ble_service.o \
$(OUT_PATH)/i2c.o \
$(OUT_PATH)/flash.o \
$(OUT_PATH)/time.o \
$(OUT_PATH)/epd_spi.o \
$(OUT_PATH)/epd.o \
$(OUT_PATH)/epd_bwry_154.o \
$(OUT_PATH)/slides.o \
$(OUT_PATH)/nfc_probe.o \
$(OUT_PATH)/led.o \
$(OUT_PATH)/led_link.o \
$(OUT_PATH)/hwinfo.o \
$(OUT_PATH)/wdt.o \
$(OUT_PATH)/textscr.o \
$(OUT_PATH)/txpower.o \
$(OUT_PATH)/tagname.o \
$(OUT_PATH)/nfc.o \
$(OUT_PATH)/main.o

# Each subdirectory must supply rules for building sources it contributes
$(OUT_PATH)/%.o: ./src/%.c
	@echo 'Building file: $<'
	@$(TC32_COMPILER_PATH)tc32-elf-gcc $(GCC_FLAGS) $(INCLUDE_PATHS) -c -o"$@" "$<"
