# STM32G474 PCB_text Worklog - 2026-06-21

## Architecture Notes

- MCU project: STM32G474, CMake/Ninja build, CubeMX-generated HAL layout.
- Main app entry: `Core/Src/main.c`.
- Display: ST7789 on SPI2, driver in `Core/Src/st7789.c`, resolution is landscape `320x240`.
- ADC/DAC:
  - ADC1 and ADC2 use DMA buffers in `main.c`.
  - DAC1 and DAC2 output 64-point sine wave by DMA; DAC1 short press on PE6 toggles sine/triangle.
- Timing:
  - TIM7 drives DAC trigger path.
  - TIM16 is manually reconfigured to 500 us interrupt and toggles broad GPIO masks for 1 kHz square-wave output.
- Button:
  - PE6 is active-low in current code.
  - Existing single click behavior is preserved for DAC1 waveform switching.
- RTC:
  - CubeMX generated `Core/Src/rtc.c` and `Core/Inc/rtc.h`.
  - RTC clock source is LSE, 24-hour mode, prescaler 127/255.

## Changes Made

- Time display now reads RTC with `HAL_RTC_GetTime()` and `HAL_RTC_GetDate()` instead of displaying the TIM16 software counter.
- Dashboard static drawing was moved into `Dashboard_DrawStatic()` so the UI can be restored after tests.
- PE6 button handling was changed to a small state machine:
  - Short click: toggle DAC1 sine/triangle waveform.
  - Long press for 5 seconds: enter LCD refresh test.
  - In LCD refresh test: single click exits and returns to dashboard.
- Added `ScreenRefreshTest_Run()`:
  - full-screen color fills,
  - vertical color bars,
  - checker/fill stress pattern,
  - 250 ms pattern step.
- CMake fix:
  - `STM32_Drivers` object library now receives `${STM32_Drivers_Src}` directly in `add_library(...)`; this avoids `cube-cmake` reporting `No SOURCES given to target`.

## Verification

- CMake configure passed with elevated filesystem/process access:
  - `cmake --preset Debug`
- Build passed:
  - `cmake --build --preset Debug`
- Output:
  - `build/Debug/PCB_text.elf`
  - FLASH used: 60960 B / 256 KB, 23.25%
  - RAM used: 8320 B / 128 KB, 6.35%

## Hardware Access Notes

- ST-Link download should use the generated ELF:
  - `build/Debug/PCB_text.elf`
- OpenOCD is available at:
  - `C:\DevEnv\DevEnv\openocd-v0.12.0-i686-w64-mingw32\bin\openocd.exe`
- Existing board config:
  - `st_nucleo_g4.cfg`
- Requested serial port:
  - `COM13`
- 2026-06-21 hardware access result:
  - OpenOCD programmed `build/Debug/PCB_text.elf`, verify OK, target reset OK.
  - OpenOCD warned `Target voltage: 0.000000`, but still detected the STM32G4 target and completed programming.
  - `COM13` opened successfully at 115200 8N1.

## TODO / Risks

- On-board visual confirmation:
  - RTC starts from CubeMX-configured time and advances on display.
  - PE6 short click still toggles DAC1 waveform.
  - PE6 5 second long press enters LCD refresh test.
  - PE6 single click exits refresh test.
- Confirm PE6 electrical pull state on hardware. Current code assumes active-low and `GPIO_NOPULL`.
- CubeMX currently initializes RTC time/date every boot; if backup-domain persistence is wanted, add backup-register guard around `HAL_RTC_SetTime()` and `HAL_RTC_SetDate()`.
- TIM16 still maintains the old software runtime counter internally, but dashboard no longer displays it. It can be cleaned later after validating no other code depends on it.
