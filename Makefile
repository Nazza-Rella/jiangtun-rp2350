.PHONY: all clean

ifeq ($(OS),Windows_NT)
    RM = powershell -Command Remove-Item -Recurse -Force
	CP = copy
    FIXPATH = $(subst /,\,$1)
	PIO = $(USERPROFILE)\.platformio\penv\Scripts\platformio.exe
else
    RM = rm -rf
	CP = cp
    FIXPATH = $1
	PIO = $(HOME)/.platformio/penv/bin/platformio
endif

VERSION = v2.0.0-alpha

.pio/build/pico/firmware.uf2: src/main.cpp src/jiangtun.h
	$(PIO) run --environment pico

.pio/build/dol-pico/firmware.uf2: src/main.cpp src/jiangtun.h
	$(PIO) run --environment dol-pico

.pio/build/xiao-rp2040/firmware.uf2: src/main.cpp src/jiangtun.h
	$(PIO) run --environment xiao-rp2040

.pio/build/dol-xiao-rp2040/firmware.uf2: src/main.cpp src/jiangtun.h
	$(PIO) run --environment dol-xiao-rp2040

.pio/build/pico2/firmware.uf2: src/main.cpp src/jiangtun.h
	$(PIO) run --environment pico2

.pio/build/dol-pico2/firmware.uf2: src/main.cpp src/jiangtun.h
	$(PIO) run --environment dol-pico2

.pio/build/xiao-rp2350/firmware.uf2: src/main.cpp src/jiangtun.h
	$(PIO) run --environment xiao-rp2350

.pio/build/dol-xiao-rp2350/firmware.uf2: src/main.cpp src/jiangtun.h
	$(PIO) run --environment dol-xiao-rp2350

dist:
	mkdir dist

dist/jiangtun-$(VERSION)-pico.uf2: .pio/build/pico/firmware.uf2 dist
	$(CP) $(call FIXPATH,$<) $(call FIXPATH,$@)

dist/jiangtun-$(VERSION)-dol-pico.uf2: .pio/build/dol-pico/firmware.uf2 dist
	$(CP) $(call FIXPATH,$<) $(call FIXPATH,$@)

dist/jiangtun-$(VERSION)-xiao-rp2040.uf2: .pio/build/xiao-rp2040/firmware.uf2 dist
	$(CP) $(call FIXPATH,$<) $(call FIXPATH,$@)

dist/jiangtun-$(VERSION)-dol-xiao-rp2040.uf2: .pio/build/dol-xiao-rp2040/firmware.uf2 dist
	$(CP) $(call FIXPATH,$<) $(call FIXPATH,$@)

dist/jiangtun-$(VERSION)-pico2.uf2: .pio/build/pico2/firmware.uf2 dist
	$(CP) $(call FIXPATH,$<) $(call FIXPATH,$@)

dist/jiangtun-$(VERSION)-dol-pico2.uf2: .pio/build/dol-pico2/firmware.uf2 dist
	$(CP) $(call FIXPATH,$<) $(call FIXPATH,$@)

dist/jiangtun-$(VERSION)-xiao-rp2350.uf2: .pio/build/xiao-rp2350/firmware.uf2 dist
	$(CP) $(call FIXPATH,$<) $(call FIXPATH,$@)

dist/jiangtun-$(VERSION)-dol-xiao-rp2350.uf2: .pio/build/dol-xiao-rp2350/firmware.uf2 dist
	$(CP) $(call FIXPATH,$<) $(call FIXPATH,$@)

all: dist/jiangtun-$(VERSION)-pico.uf2 dist/jiangtun-$(VERSION)-dol-pico.uf2 dist/jiangtun-$(VERSION)-xiao-rp2040.uf2 dist/jiangtun-$(VERSION)-dol-xiao-rp2040.uf2 dist/jiangtun-$(VERSION)-pico2.uf2 dist/jiangtun-$(VERSION)-dol-pico2.uf2 dist/jiangtun-$(VERSION)-xiao-rp2350.uf2 dist/jiangtun-$(VERSION)-dol-xiao-rp2350.uf2

clean:
	$(RM) $(call FIXPATH,dist/*)
	$(RM) $(call FIXPATH,.pio/build/*)
