# Migration: coffee_ctrl_main (Arduino) → bananactrl (ESP-IDF v6, modern C++)

Source: `~/Programmierprojekte/Arduino/coffee_ctrl_main` (Arduino-ESP32 core 2.x, board `esp32thing_plus`, partition `large_spiffs`)
Target: this repo, plain ESP-IDF v6 + CMake, no Arduino component, **C++ throughout, object-oriented design**.

Strategy: **keep the behaviour, change the structure.** The firmware must do exactly what the Arduino
version does (same web UI, same `params.json`, same CSV). The code, though, is rebuilt as classes with clear
ownership instead of globals + `setup()`/`loop()`. Each phase ends with something that builds and
runs on the board. Keep the Arduino firmware on a second board (or keep the `.bin`) as a reference.

> ESP-IDF v6.1 is installed at `/opt/esp/esp-idf-v6.1` (tools in `~/.espressif`). Points marked
> **[v6-check]** are 5.x→6.x changes to check against the official *Migration Guides → 6.0* page
> (and the header files under `/opt/esp/esp-idf-v6.1/components`) before you implement them.

---

## 1. C++ design rules (apply in every phase)

**Language & tooling**
- All project code is `.cpp`/`.hpp`. The only C left is ESP-IDF itself, and it is always hidden behind a class.
- ESP-IDF v6.1 compiles C++ with `-std=gnu++26` (GCC 15.2) by default, which was checked on this machine.
  No override is needed; `std::expected`, `std::span` and `std::format` are all available.
  **Avoid `<format>` in firmware code:** it pulls in ~300 KB of libstdc++ locale code. Use `snprintf`.
- Commit `.clang-format` and `.clang-tidy` (`modernize-*`, `cppcoreguidelines-*`, `readability-*`, `bugprone-*`).
- Everything lives in `namespace banana` with sub-namespaces per layer (`banana::hal`, `banana::drivers`, …).

**Ownership & lifetime**
- **No mutable globals.** One composition root (`App`) owns every object as a member. `app_main()`
  only does `static banana::App app; app.run();`.
- Dependencies are passed **by reference in the constructor** (constructor injection), never looked up globally.
- **RAII for every IDF handle.** Examples: I2C bus/device, LEDC channel, `esp_timer`, httpd, mounted
  FS, open file, OTA handle, mutex and task. The destructor releases the handle. Wrappers are
  non-copyable; make them movable only where it helps.
- No raw `new`/`delete`. Use `std::unique_ptr` where heap allocation is needed. Keep the control
  path allocation-free (`std::array` instead of `float**` tables).

**Error handling**
- Exceptions stay off (the IDF default, which keeps binaries small), so failures are values:
  `std::expected<T, esp_err_t>` (alias `banana::Result<T>`). `Result<void>` is for operations with
  no return value.
- Constructors can't fail. Objects that need hardware init use a **static factory**:
  `static Result<Ads1115> create(hal::I2cBus&, Ads1115::Config)`.
- `ESP_ERROR_CHECK` only in `App` for truly fatal boot errors.

**Types**
- `enum class` for state, error and colour. Bit flags go through a small `Flags<E>` helper
  or `std::bitset`, not raw `unsigned int`.
- Pins and constants as `constexpr` in a `banana::board` namespace (`constexpr gpio_num_t kSsrPwm = GPIO_NUM_21;`).
- `std::string`/`std::string_view` instead of `String`/`const char*`. Use `std::span` for buffers
  and `std::chrono` durations for times (`200ms`, `450ms`, `3600s`).
- `const`-correct getters, `[[nodiscard]]` on everything that returns `Result`.

**Polymorphism where it pays**
- Interfaces (pure virtual) at the hardware boundary so logic can be tested on the host with fakes:
  `ITemperatureSensor`, `IPwmOutput`, `IClock`.
- **Strategy pattern** replaces `#ifdef Pt1000_CONV_*`: `ITemperatureConverter` →
  `LinearConverter`, `QuadraticConverter`, `LookupTableConverter`.
- No virtual calls or heap in ISRs.

**RTOS & C callbacks**
- `rtos::Task` base class: `start(name, stack, prio)` → `xTaskCreate` with a static trampoline,
  and the derived class overrides `void run()`.
- Every C callback (GPIO ISR, `esp_timer`, event handler, httpd handler, log vprintf) goes through a
  `static` member trampoline with `this` as `void* arg`.
- ISRs are `IRAM_ATTR` and do nothing except notify a task / set an event bit.
- Shared state: `std::mutex` + `std::scoped_lock` (supported via IDF's pthread layer) or an
  `rtos::Mutex` wrapper. Use `portMUX` spinlocks only for data shared with an ISR.

---

## 2. Target architecture

```
                    ┌──────────────────────── App (composition root) ─────────────────────────┐
                    │ owns all objects below, wires dependencies, starts tasks                 │
                    └──────────────────────────────────────────────────────────────────────────┘
 services     WifiManager  MdnsService  TimeSync  WebServer ─┬─ StaticFileRoutes
                                                            ├─ ApiRoutes (lastvalues, params)
                                                            └─ OtaRoutes ── OtaUpdater
 control      HeaterController (rtos::Task)   ── PidController, Diagnostics, StatusIndicator
              BrewDetector                     ── debounce pump relay
              MeasurementRecorder              ── data.csv
 config       Config (value type)   ConfigStore (JSON ⇄ Config, ArduinoJson)
 storage      LittleFs (RAII mount)  File (RAII)  FileLogger (rtos::Task, log sink)
 drivers      Ads1115 : ITemperatureSensor ── ITemperatureConverter (Linear|Quadratic|Lut)
              Ssr : IPwmOutput     RgbLed (3× PwmChannel + gains)
 hal          GpioOutput  GpioInput(+ISR)  I2cBus  I2cDevice  LedcTimer  PwmChannel
              PeriodicTimer(esp_timer)  EventGroup  Mutex  Task  Clock
```

Dependencies only point downwards. `hal` knows nothing about coffee, and the `control` layer knows
nothing about ESP-IDF (only interfaces), so it can be unit-tested on the host.

Mapping of the Arduino globals and functions to classes:

| Arduino (`coffee_ctrl_main.ino`)                             | New owner                                         |
|--------------------------------------------------------------|---------------------------------------------------|
| `objConfig`, `resetConfiguration/load/saveConfiguration`     | `Config` (defaults as member initialisers) + `ConfigStore` |
| `bParamFileLocked`                                           | `std::mutex` inside `ConfigStore`                 |
| `iState`, `objTimerMux`, `onTimerLong`, `onAlertRdy`         | `HeaterController` + `EventGroup` + `PeriodicTimer` |
| `onPumpRelayChange`, `pump_relay_last_interrupt_time`        | `BrewDetector`                                    |
| `controlHeating()`, `objPid`, `fTarPwm`, `bTimeOutReached`   | `HeaterController`, `PidController`, `Ssr`        |
| `setColor()`, `configLED()`, LED channels                    | `RgbLed` (hardware) + `StatusIndicator` (which colour when) |
| DIAG block in `loop()`, `iErrorId`                           | `Diagnostics` with `enum class Fault` flags       |
| `writeMeasFile()`, CSV header in `setup()`                   | `MeasurementRecorder`                             |
| `connectWiFi()`, `calcWifiStrength()`, SoftAP fallback       | `WifiManager`                                     |
| `configWebserver()`                                          | `WebServer` + route classes                       |
| OTA lambdas, `Update.h`                                      | `OtaUpdater` (RAII around `esp_ota_handle_t`)     |
| `vprintf_into_FS()`                                          | `FileLogger`                                      |
| `fTime`, `fTemp` shared with web server                      | `HeaterController::snapshot()` → `ProcessSnapshot` value struct |

---

## 3. Inventory — API replacements

| Arduino / library                            | ESP-IDF v6 replacement (wrapped in)                                                    |
|----------------------------------------------|----------------------------------------------------------------------------------------|
| `millis()`, `delay()`                        | `esp_timer_get_time()` → `hal::Clock` (`std::chrono`), `vTaskDelay`                    |
| `Serial.print*`                              | `ESP_LOGx`                                                                             |
| `pinMode/digitalWrite/digitalRead`           | `gpio_config()` etc. → `hal::GpioOutput` / `hal::GpioInput` (`esp_driver_gpio`)         |
| `attachInterrupt`                            | `gpio_isr_handler_add()` → `hal::GpioInput::onEdge()`                                   |
| `hw_timer_t` (450 ms)                        | `esp_timer` periodic → `hal::PeriodicTimer`                                            |
| `ledcSetup/ledcAttachPin/ledcWrite`          | `ledc_timer_config/ledc_channel_config/ledc_set_duty` → `hal::LedcTimer`, `hal::PwmChannel` |
| legacy `driver/i2c.h` in ADS1115             | `driver/i2c_master.h` → `hal::I2cBus`, `hal::I2cDevice` **[v6-check: legacy removed]** |
| `WiFi.h`                                     | `esp_wifi` + `esp_netif` + event loop → `WifiManager`                                  |
| `ESPmDNS`                                    | `espressif/mdns` → `MdnsService`                                                       |
| `configTime/getLocalTime`                    | `esp_netif_sntp_*` + TZ `CET-1CEST,M3.5.0,M10.5.0/3` → `TimeSync`                      |
| `LittleFS`                                   | `joltwallet/littlefs` + POSIX → `storage::LittleFs`, `storage::File`                   |
| `ArduinoJson` v7                             | keep: `bblanchon/arduinojson` (it's C++, works with `std::string`)                     |
| `ESPAsyncWebServer` + `AsyncTCP`             | `esp_http_server` → `WebServer` + route classes                                        |
| `Update.h`                                   | `esp_ota_ops.h` → `OtaUpdater`                                                         |
| `ota.h` (hex array)                          | `EMBED_FILES` (gzip at build time) → `std::span<const uint8_t>` accessor               |
| `esp_task_wdt_init(75, true)`                | `esp_task_wdt_reconfigure` (75 s) + `esp_task_wdt_config_t` → `rtos::Watchdog` (RAII subscribe/unsubscribe) |
| `ESP.restart()`                              | `esp_restart()` (delayed via one-shot `PeriodicTimer`)                                 |
| `WifiAccess.h`, `Pt1000.h` (git-ignored)     | `Kconfig.projbuild` + git-ignored `sdkconfig.defaults.local`                           |

Hardware constants go into `board.hpp` as `constexpr gpio_num_t`: SSR 21, LED R/G/B 13/27/12, status
LED 33, pump relay 17, SDA/SCL 23/22, ADS ALERT/RDY 14. `A0` (used as a "3.3 V source" output) is a
board-variant alias. Look it up in the core's `variants/esp32thing_plus/pins_arduino.h` and write the
real GPIO number. It must not be one of the input-only pins 34–39.

---

## Phase 1 — Skeleton, tooling, HAL basics (1 day)

1. Install ESP-IDF v6 (EIM installer or `git clone -b v6.0 --recursive` + `./install.fish esp32`),
   `. ./export.fish`, `idf.py set-target esp32`.
2. Layout — one IDF component per layer, so dependencies are enforced by CMake `REQUIRES`:
   ```
   bananactrl/
   ├── CMakeLists.txt  partitions.csv  sdkconfig.defaults  sdkconfig.defaults.local (ignored)
   ├── .clang-format  .clang-tidy
   ├── main/                         app_main.cpp, App.hpp/.cpp, Kconfig.projbuild, idf_component.yml
   ├── components/
   │   ├── banana_core/              Result.hpp, Flags.hpp, board.hpp
   │   ├── banana_hal/               Gpio*, I2c*, Ledc*, PeriodicTimer, Clock, rtos/{Task,Mutex,EventGroup,Watchdog}
   │   ├── banana_drivers/           Ads1115, TemperatureConverters, Ssr, RgbLed
   │   ├── banana_control/           PidController, HeaterController, BrewDetector, Diagnostics, StatusIndicator
   │   ├── banana_config/            Config, ConfigStore
   │   ├── banana_storage/           LittleFs, File, FileLogger, MeasurementRecorder
   │   └── banana_net/               WifiManager, MdnsService, TimeSync, WebServer, routes/, OtaUpdater
   ├── test/host/                    Unity/GoogleTest on linux target, fakes for interfaces
   └── data/                         web UI → LittleFS image
   ```
3. `partitions.csv` (Thing Plus has 16 MB flash):
   ```
   # Name,   Type, SubType, Offset,  Size
   nvs,      data, nvs,     0x9000,  0x5000
   otadata,  data, ota,     0xe000,  0x2000
   app0,     app,  ota_0,   0x10000, 0x200000
   app1,     app,  ota_1,   ,        0x200000
   storage,  data, littlefs,,        0x800000
   ```
   `sdkconfig.defaults`: `CONFIG_PARTITION_TABLE_CUSTOM=y`, `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`,
   `CONFIG_ESPTOOLPY_FLASHFREQ_80M=y`, `CONFIG_ESP_TASK_WDT_TIMEOUT_S=60` (Kconfig range is 1–60; 75 s is set at runtime by `rtos::Watchdog`), `CONFIG_ESP_TASK_WDT_PANIC=y`,
   `CONFIG_COMPILER_CXX_EXCEPTIONS=n`, `CONFIG_COMPILER_CXX_RTTI=n`, `CONFIG_HTTPD_MAX_REQ_HDR_LEN=1024`.
4. Explicit `REQUIRES` per component (v6 no longer pulls in the umbrella `driver` component **[v6-check]**),
   e.g. `banana_hal` → `esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_timer freertos`.
5. Write `banana_core` (`Result<T>`, `Flags<E>`, `board.hpp`) and the first HAL classes:
   `hal::GpioOutput`, `rtos::Task`, `storage::LittleFs` (RAII mount, `info()` → total/used bytes).
   Add `littlefs_create_partition_image(storage ../data FLASH_IN_PROJECT)`.

✅ **Exit:** `App` constructs, mounts LittleFS, lights the status LED via `GpioOutput`, logs the FS info.
clang-tidy passes.

---

## Phase 2 — Pure logic classes + host tests (1 day) ✅

1. **`control::PidController`** from `PidCtrl`:
   - `begin(float*, float*)` with raw pointers to globals goes away. The API is
     `float compute(float actual, std::chrono::milliseconds dt)`, with no hidden time source.
   - `struct Gains { kp, ki, kd }` (+ `Gains::fromTimeConstants(kp, tn, tv)`), `struct Terms`,
     `struct Limits`, `struct Thresholds { std::optional<float> on, off; }` replace the
     activate/deactivate pairs.
   - **Dropped:** the gain-schedule table (`changePidCoeffs(float[][4], size_t)`). The firmware never
     used it, it read past the end of its table and mixed up gain and time-constant units.
     Add it back as `std::span<const GainSegment>` if it is ever needed.
2. **`control::BrewFeedForward`** — the open-loop brewing output from `controlHeating()`
   (`BrewFfStart/End/Tau/Gain`), which the original plan did not list.
   `control::toPidSettings()` / `toBrewFeedForwardSettings()` replace `configPID()`.
3. **Temperature conversion** → `ITemperatureConverter` with `LinearConverter`, `QuadraticConverter`,
   `LookupTableConverter(std::span<const Point>)` (`consteval`: a bad table does not compile).
   Data in `banana/drivers/Pt1000.hpp`, generated from `Pt1000.h`; Kconfig choice
   `BANANA_PT1000_*` (default: 5.0 V lookup table). Findings from the tests:
   - the Arduino "3.3 V" lookup table matches a **5.08 V** bridge supply;
   - the linear/quadratic coefficients are fits for a **3.3 V** bridge (worst case 10.8 K / 1.03 K)
     and are about 50 K off on the 5 V hardware.
4. **`config::Config`** as a plain value struct with default member initialisers (replaces
   `resetConfiguration(false)`), grouped: `WifiSettings`, `PidSettings` (incl. `BrewFeedForwardSettings`),
   `SsrSettings`, `LedSettings`, `SignalSettings`, `SystemSettings`. Empty Wi-Fi credentials mean
   "factory credentials" (filled in by `ConfigStore` in Phase 6, Kconfig in Phase 5).
5. Host tests in `test/host` (plain CMake + GoogleTest):
   `cmake -S test/host -B build-host -G Ninja && cmake --build build-host && ctest --test-dir build-host`.
   - `PidController` runs next to the **unchanged Arduino `PidCtrl.cpp`** (with a `millis()` shim) on
     the recorded heat-up `rancilio_silvia_stock_measurement.csv` (6231 samples, 4 configurations incl.
     D-term, thresholds and resets); outputs agree within 1e-3 counts.
   - Converters against `PT_1000_tabelle.csv` through the bridge model of `calc_wheat_stone.py`.
   - `BrewFeedForward`, `Config` defaults vs. `resetConfiguration()`, config mapping, `Flags`.

✅ **Exit:** logic classes build for both `esp32` and host; 32 host tests green; clang-tidy clean.

---

## Phase 3 — I2C HAL + `Ads1115` driver (1 day) ✅

1. `hal::I2cBus` (RAII `i2c_new_master_bus`, `probe()`, `addDevice()`), `hal::I2cDevice` (RAII
   `i2c_master_bus_add_device`, `write(std::span<const uint8_t>)`, `writeRead(...)`),
   `hal::GpioInput` (pull mode, `onEdge(Edge, IsrCallback, void*)`).
2. `drivers::Ads1115 : ITemperatureSensor` takes `hal::I2cDevice&` and `const ITemperatureConverter&`.
   - `static Result<Ads1115> create(...)` reads the config register (presence check, which the Arduino
     driver never did: `_bConnectStatus` was never set) and applies the settings.
   - `ads1115::ConfigRegister` value class with typed enums (`Mux`, `Pga`, `DataRate`, ...) replaces the
     16 bit-level `setX/getX` pairs; `kCoffeeMachineConfig` = `0x880C`, the register value that
     `configADS1115()` produced. `configure()` writes it in one transfer and verifies the read-back
     (ignoring the OS bit, which reads 0 in continuous mode).
   - Filter → `MovingAverage<12>` (`std::array` ring buffer). The Arduino Savitzky-Golay branch only
     existed for buffer sizes 5/7/9/11, so with size 12 it was always the moving average.
   - `printConfigReg()` → `ConfigRegister::toString()` (with `snprintf`, see §1).
3. The ALERT/RDY pin is **not** part of the driver: `hal::GpioInput` (internal pull-up) with `onEdge()`
   is wired in `App`. Until Phase 4, `SensorMonitor` (main/) reads on every edge and logs once per second.

Bench notes: an ADS1115 that answers on I2C but resets to its power-on defaults as soon as it converts
has no proper VDD/GND (it is powered through the I2C pull-ups). A module that delivers **~128
conversions/s at "8 SPS"** carries a 12-bit ADS1015, not an ADS1115.

✅ **Exit:** on the bench (AIN0/AIN1 on GND) the ADS1115 is configured to `0x880C`, delivers 8.1 SPS via
the ready interrupt, raw 0 → 77.13 °C (the 5 V lookup table at 0 V). Temperature comparison with the
Pt1000 against the Arduino firmware is still open (needs the machine board, see Phase 8).

---

## Phase 4 — Actuators and control (2 days)

1. HAL: `hal::LedcTimer` (freq, resolution; `reconfigure()`), `hal::PwmChannel` (`setCounts()`, duty in
   counts like `ledcWrite()`), `hal::PeriodicTimer` (RAII `esp_timer`, plain callback + `void*`),
   `rtos::EventGroup`, `rtos::Watchdog` (`configure(75 s, panic)` + RAII subscription of the calling task).
2. Hardware interfaces in `banana_core/io` so that `control` depends on nothing below it:
   `ITemperatureSensor` (+ `healthy()`, `setFilterActive()`), `IPwmOutput`, `IStatusLed`, `IDigitalInput`,
   `INetworkStatus`. Host tests use fakes (`test/host/fakes.hpp`, `shim/esp_err.h`).
3. Drivers: `drivers::Ssr : IPwmOutput` on LEDC timer 0 (15 Hz). `drivers::RgbLed : IStatusLed` owns three
   `PwmChannel`s on LEDC timer 1 (500 Hz); the colour mixing of `setColor()` is the pure `rgbCounts()`.
4. Control (all pure, host-tested):
   - `control::indicate()` — `(Faults, brewing, temp, target) → LedCommand`.
   - `control::BrewDetector` — first edge starts a 200 ms window, then the pin level decides. The ISR only
     sets an event bit; the task timestamps it.
   - `control::diagnose()` — temp < 10 °C, ADC not in continuous mode / not answering, Wi-Fi → `Flags<Fault>`.
   - `control::ControlLoop` — the Arduino `loop()`/`controlHeating()` without RTOS: `onSample()` (8 SPS,
     PID or brew feed-forward on every 3rd), `onTick()` (450 ms: LED every 3rd, DIAG every 2nd), standby.
5. `HeaterTask : rtos::Task` (main/) instead of `HeaterController` in `control` (keeps `control` free of
   FreeRTOS): waits on `EventGroup` bits (`SampleReady`, `Tick`, `PumpEdge`, `ConfigChanged`), calls the
   `ControlLoop` in the Arduino order, feeds the watchdog, publishes `ProcessSnapshot` under a `std::mutex`.
   `requestConfig()` from other tasks → applied inside the task (SSR/LED timers + `ControlLoop::applyConfig()`).
   Faults, brewing and standby are logged on change (Arduino repeated fault messages every 0.9 s).
6. Arduino bugs that the new types fix on their own:
   - `NO_ERROR = 1u << 0` is a bit, so `iErrorId == WIFI_DISCONNECT` is never true.
     With `Flags<Fault>` + `none()`/`only(Fault::WifiDisconnect)` this can't happen.
   - `/paramReset` reconfigures the PID from the web task → now only a `ConfigChanged` event.
   - Unprotected `&= ~BREWING_DETECTION` / `iInterruptCntPump++` → event group is atomic.
7. Kept on purpose (Arduino behaviour): standby counts from boot, not from the last use, and lasts until
   reboot.
8. Changed on purpose:
   - A Wi-Fi fault only turns the LED purple; only temperature and ADC faults switch the heater off
     (`control::blocksHeating()`). The Arduino firmware stopped heating on any fault.
   - Safety fix (found in the bench fault test): the Arduino firmware wrote the SSR only in the PID step,
     which the ADC's ready pulse triggers, so a dead ADC left the last duty on the heater. Now a blocking
     fault or standby switches the heater off at once, and no conversion for 1 s counts as ADC fault.

✅ **Exit:** on the bench (heater through a dummy load), LED colours, SSR duty, brew detection and standby
timeout behave like the Arduino firmware. Only then connect the machine.

---

## Phase 5 — Networking (1–2 days) ✅

1. `net::WifiManager : io::INetworkStatus` — `Result<Mode> start(Credentials, 18 s)` tries STA (as
   `connectWiFi(3, 6000)`) and falls back to the open SoftAP `SilviaCoffeeCtrl` (192.168.4.1). Owns netif,
   default event loop and Wi-Fi driver; not movable (the event handlers hold `this`). After a successful join
   it reconnects on every `WIFI_EVENT_STA_DISCONNECTED`; disconnect reasons are logged with the RSSI.
   `rssiPercent()` (`calcWifiStrength()`, now live instead of once at boot), `ipAddress()`.
   `stationDisconnected()` feeds Diagnostics (LED purple, heating continues, see Phase 4).
2. `net::MdnsService` (RAII `mdns_init/free`, hostname `coffee`, `_http._tcp` 80), component `espressif/mdns`.
3. `net::TimeSync` — `esp_netif_sntp_*` with `europe.pool.ntp.org` and TZ `CET-1CEST,M3.5.0,M10.5.0/3`
   (the fixed `+3600/+3600` offsets were wrong in winter), `std::optional<std::tm> now()`. Station mode only.
4. `storage::Nvs` (RAII `nvs_flash_init`, erase on version change): the Wi-Fi driver needs NVS.
5. Factory credentials: `Kconfig.projbuild` → `CONFIG_BANANA_WIFI_FACTORY_SSID/PW`, values in the
   git-ignored `sdkconfig.defaults.local`. Used when `Config::wifi.ssid` is empty.
6. Boot order changed: the heater starts first, Wi-Fi afterwards (the Arduino `setup()` blocked up to 18 s
   in `connectWiFi()` before the heater was configured).

✅ **Exit:** SoftAP fallback and mDNS verified on the bench (the board has a u.FL module without antenna,
RSSI -91 dBm, so the station join failed with reason 2 "auth expire"). Station join, `coffee.local` in the home
network and local time were not tested on hardware (skipped).

---

## Phase 6 — Storage, config, logging (1 day) ✅

1. `storage::File` — RAII `FILE*` (`open(path, mode) → Result<File>`, `write()`, `readAll()`, `flush()`),
   `exists()`, `remove()`, `rename()`. All paths under `/fs`. **`flush()` = `fflush()` + `fsync()`:**
   LittleFS commits only on sync/close, so without `fsync()` a reset lost everything written since opening
   (seen on the bench). The Arduino firmware closed the file after every write.
2. `config::ConfigJson` — pure codec `toJson(Config)` / `fromJson(json, base) → {Config, complete}` with the
   **exact key names** (incl. `HighTresholdValue`). Missing keys keep the `base` value (defaults when loading,
   current values for a partial `/paramUpdate`). Keys are tested with `isNull()`: the Arduino firmware used
   the value's truthiness, so stored `false`/`0` (e.g. `CtrlPropActivate`, `SigFilterActive`) were replaced by
   the defaults on every boot.
3. `config::ConfigStore` — `load()` (never fails: defaults + write back if the file is missing/invalid or keys
   are missing), `save()` (temporary file + atomic LittleFS rename), `reset()`. A `std::mutex` replaces
   `bParamFileLocked`. Wi-Fi SSID empty = factory credentials (the Arduino firmware wrote the factory SSID and
   password into params.json).
4. `storage::MeasurementRecorder` + `storage::csv` — data.csv byte-compatible with the Arduino file
   (7 header lines, CR LF, same number formats; graphs.html parses it by line index). Created after the time
   sync; `HeaterTask` appends a row on every 450 ms tick. Timestamp `unknown` without NTP (the Arduino firmware
   wrote an uninitialised buffer).
5. `storage::FileLogger : rtos::Task` — `esp_log_set_vprintf` hook (static instance pointer: the hook has no
   user argument) forwards to the UART, formats into a 256-byte stack buffer and pushes into an 8 KB ring
   buffer; the task appends to `/fs/logfile_recent.txt`. Rotates `recent` → `last` at start and when recent
   exceeds 512 KB. Started right after mounting, so boot messages end up in the file.
   [v6-check done: `esp_log_set_vprintf()` unchanged in v6.1; log v1 emits one call per line.]

✅ **Exit:** params.json written with defaults and reloaded on the next boot, data.csv and both log files
present after a reset (checked by reading the partition back). Download via HTTP follows in Phase 7.

---

## Phase 7 — Web server + OTA (2 days) — implemented, web UI test pending

New component `banana_web` (depends on config, control, net, storage).

1. `web::WebServer` — RAII around `httpd_handle_t` (wildcard matching, 16 handlers, 8 kB stack,
   `lru_purge_enable` because browsers keep idle sockets). Instead of an `IRoute` interface, each route class
   has `registerOn(WebServer&)` and uses `server.on<Class, &Class::method>(uri, method, *this)`: a template
   trampoline turns `user_ctx` back into the object. `HttpRequest` wraps `httpd_req_t*`
   (`header()`, `body(max)`, `receive(buffer)` with timeout retries, `send()`, chunked sending).
2. Route classes:

   | Class               | URIs                                                                 | Depends on                    |
   |---------------------|----------------------------------------------------------------------|-------------------------------|
   | `ApiRoutes`         | `GET /lastvalues.json`, `GET /params.json`, `POST /paramUpdate`, `GET /paramReset`, `GET /restartesp` | `control::IHeaterControl` (implemented by `HeaterTask`), `ConfigStore`, `WifiManager` |
   | `OtaRoutes`         | `GET /failsafe`, `POST /ota_firmware`, `POST /ota_spiffs`            | `OtaUpdater`, LittleFS        |
   | `StaticFileRoutes`  | `GET /*` (registered last): the files in `kStaticFiles` only         | LittleFS (4 kB chunks)        |

   - **Wi-Fi password never leaves the device:** `/params.json` is generated from `ConfigStore::current()` by
     `toPublicJson()` (password empty), not served from the file; `params.json` is not in `kStaticFiles`.
     `/paramUpdate` goes through `applyWebUpdate()`: an empty password keeps the stored one.
     `settings.html` shows the field as `type="password"` with the placeholder "unchanged".
     (Consequence: an open network cannot be set through the web UI once a password is stored.)
   - `/paramUpdate` merges into the current configuration (missing keys keep their value; the Arduino firmware
     set them to 0), saves, then `IHeaterControl::requestConfig()`. Wi-Fi changes are reported as
     "take effect after a restart" (as before, only applied at boot).
   - `/lastvalues.json`: same keys as before (`Time`, `Temperature`, `PID.*`, `WiFi."SignalStrength in %"`).
   - Response texts kept where the pages show them (`alert()`).
3. `web::OtaUpdater` — `begin(md5, size)` (`OTA_WITH_SEQUENTIAL_WRITES`: sectors are erased as they are
   reached, not the whole 2 MB up front), `write()`, `finish()` (MD5 via `esp_rom_md5`, then `esp_ota_end()`
   validates the image, then `esp_ota_set_boot_partition()`). The destructor calls `esp_ota_abort()` if
   `finish()` wasn't reached. The MD5 stays mandatory, as in the Arduino firmware.
4. `ota.html` sends the file with `fetch()` as the raw body with `X-MD5` / `X-Filename` headers (no multipart
   parser in `esp_http_server`). `/failsafe` is the same `ota.html`, gzip-compressed at build time
   (`components/banana_web/CMakeLists.txt`) and embedded — `ota.h` with the hand-made hex dump is gone.
   `/ota_spiffs` (name kept) writes `.<name>.upload` and renames it. Allowed names: `[A-Za-z0-9._-]`,
   max 32 characters, no leading dot, not `params.json`/`data.csv`/log files; max 512 kB.
5. `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. `App::startWebServer()` calls
   `OtaUpdater::markRunningAppValid()` once the web server runs (SoftAP counts). Needs the new bootloader,
   i.e. one serial `idf.py flash`.
6. The Arduino `server.end(); server.begin();` on `WIFI_DISCONNECT` is gone: `esp_http_server` is bound to
   all interfaces and survives reconnects.

Tests: host tests for `WebPaths` (URI → file, MIME, upload names, MD5 parsing), `lastValuesJson`,
`toPublicJson` / `applyWebUpdate` (incl. numbers sent as strings by `settings.html`).

Bench (SoftAP, no Wi-Fi client available yet): web server starts at 192.168.4.1, routes registered,
image 1.03 MB (50 % of the OTA slot free).

**Bench finding — I2C NACK during flash writes (mitigated):** an ADS1115 read occasionally ends in a NACK
("I2C bus is still busy but software timeout detected"; the driver resets the bus). Experiments:
- without `data.csv` recording, or with the FileLogger not writing: no errors (100 s);
- recording + FileLogger: one error per ~9 s; with an extra log line per tick (more FileLogger flushes): one per
  ~1.3 s — always a conversion read right after ALERT/RDY, never the config read at tick time;
- `CONFIG_I2C_ISR_IRAM_SAFE` and a 500 ms I2C timeout: no change. ADS1115 VDD is 3.3 V.

So a read fails when a flash write from *another* task (other core) overlaps it; the heater task's own
`data.csv` writes never overlap its own reads. Root cause inside the ESP32/IDF I2C path not identified.
Mitigation: `Ads1115::readRegister()` retries once — under the stress build 109 retries in 150 s, none failed
twice; with the normal build no fault in 90 s. The driver's `E i2c.master` lines remain in the log.

Also measured: each `data.csv` append + `fsync` takes 43–49 ms (LittleFS rewrites the partial last block;
grows with the block fill), every ~6–13 s one takes up to 320 ms (metadata compaction). The heater task is
blocked meanwhile, so an ALERT/RDY event can be merged with the next one. Same write pattern as the Arduino
firmware (open/append/close per row); moving the recorder to its own task is a Phase 8 option.

✅ **Exit:** all pages in `data/` work (only the upload form changed), settings round-trip, graphs load,
OTA from the web UI boots the new image.

---

## Phase 8 — Composition, validation, cut-over (1 day)

1. `App` final form: members declared in dependency order (so destruction is reverse), `run()` does
   boot sequence → `start()` tasks → returns (`app_main` task can end).
2. Side-by-side: record `data.csv` from old and new firmware for one heat-up + one shot, compare with
   `measurements/plot_measurement.py`.
3. Soak test ≥ 24 h; `Diagnostics` also logs `esp_get_free_heap_size()` and
   `heap_caps_get_minimum_free_size()`.
4. **Note:** the first flash has to go over USB. The partition table changes, so Arduino's
   `Update.h` OTA cannot install the IDF image. Back up `params.json` from the old firmware first
   (settings page → download).
5. Optional follow-ups: bundle a chart library into LittleFS (graphs in SoftAP mode), SSE endpoint instead
   of polling, config in NVS.

---

## Rough timeline

| Phase | Content                                   | Effort |
|-------|-------------------------------------------|--------|
| 1     | Skeleton, tooling, core + first HAL       | 1 d    |
| 2     | PID, converters, Config + host tests      | 1 d    |
| 3     | I2C HAL + Ads1115                         | 1 d    |
| 4     | LEDC/GPIO/timer HAL, control classes      | 2 d    |
| 5     | WifiManager, mDNS, TimeSync               | 1–2 d  |
| 6     | ConfigStore, recorder, FileLogger         | 1 d    |
| 7     | WebServer, routes, OtaUpdater             | 2 d    |
| 8     | App composition, validation, cut-over     | 1 d    |

Phases 2, 3 and 5 are independent. Phase 4 needs 3, and Phase 7 needs 5 and 6.
