# CLAUDE.md

Temperature control firmware for a Rancilio Silvia espresso machine on a SparkFun ESP32 Thing Plus (ESP32,
16 MB flash): Pt1000 via ADS1115, PID-controlled SSR, RGB status LED, web UI (dashboard, graphs, settings,
OTA, log), optional MQTT with Home Assistant discovery. It is an ESP-IDF v6.1 / C++26 port of the Arduino
firmware `coffee_ctrl_main`; behaviour, web UI, `params.json` and `data.csv` stay compatible with it.

Further reading: [README.md](README.md) (build, flash, settings, MQTT, releases),
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) (component graph, tasks, data flow, deliberate differences
from the Arduino firmware, hardware and bench notes). Read the differences before changing control, settings
or recording behaviour.

## Layout

| Path | Content |
|---|---|
| `main/` | `App` (composition root: owns every object, wires dependencies, starts services), `HeaterTask`, `app_main.cpp`, `Kconfig.projbuild` (Pt1000 conversion, factory Wi-Fi) |
| `components/banana_core` | `Result<T>` (`std::expected<T, esp_err_t>`), `Flags`, board pins, `io` interfaces (`ITemperatureSensor`, `IPwmOutput`, ...) |
| `components/banana_hal` | RAII wrappers for GPIO, I2C, LEDC, `esp_timer`; `rtos::Task`, `EventGroup`, `Watchdog` |
| `components/banana_drivers` | ADS1115, SSR, RGB LED, Pt1000 temperature converters (strategy pattern) |
| `components/banana_control` | `ControlLoop`, `PidController`, brew detection/feed-forward, diagnostics, status indicator, `IHeaterControl` |
| `components/banana_config` | `Config` value type, JSON mapping (`params.json`), `ConfigStore` (NVS, listeners) |
| `components/banana_storage` | LittleFS, NVS, `File`, `FileLogger`, `MeasurementRecorder` / `MeasurementCsv` (`data.csv`) |
| `components/banana_net` | `WifiManager` (station / SoftAP fallback), mDNS, SNTP |
| `components/banana_web` | `WebServer`, `ApiRoutes`, `StaticFileRoutes`, `OtaRoutes`, `EventStream` (SSE `/events`), `WebPaths` (URI → file table) |
| `components/banana_mqtt` | `MqttService`, `HomeAssistant` (topics, discovery, commands) |
| `data/` | Web UI, flashed as the LittleFS image (`storage` partition); `uPlot.min.js.gz` is the bundled chart library, `footer.js` shows the firmware version |
| `test/host/` | GoogleTest unit tests for the pure-logic sources, built with the host compiler; `shim/` (ESP-IDF header stand-ins), `fakes.hpp`, `reference/` (original Arduino PID as golden reference), `data/` (measurement tables) |
| `tools/clang_tidy.py` | clang-tidy over `main/` and `components/` with Espressif's clang |
| `third_party/uplot/` | uPlot license and provenance |
| `.github/workflows/` | `build.yml` (firmware + host tests on push/PR), `release.yml` (GitHub release from `v*` tags) |
| `partitions.csv` | nvs, otadata, two 2 MB OTA slots, 8 MB LittleFS |

## Commands

```sh
. /opt/esp/esp-idf-v6.1/export.sh
idf.py build                                   # firmware + LittleFS image from data/
idf.py -p /dev/ttyUSB0 flash monitor           # full flash (erases data.csv and logs, keeps NVS settings)
idf.py -p /dev/ttyUSB0 app-flash               # app only

# host unit tests (need `idf.py build` once: ArduinoJson comes from managed_components/)
cmake -S test/host -B build-host -G Ninja && cmake --build build-host && ctest --test-dir build-host

tools/clang_tidy.py                            # after idf.py build; warnings are errors
```

`sdkconfig` is generated from `sdkconfig.defaults` + the git-ignored `sdkconfig.defaults.local` (factory
Wi-Fi credentials); after changing a defaults file, delete `sdkconfig` and rebuild.

## Code conventions

**Language and tooling**
- All project code is `.cpp`/`.hpp` (C++26, `-std=gnu++26` is the ESP-IDF v6.1 default); ESP-IDF's C APIs
  are always hidden behind a class. Everything lives in `namespace banana::<layer>`.
- Format with `.clang-format` (LLVM based, 4 spaces, 110 columns, function braces on their own line).
  Naming per `.clang-tidy`: `CamelCase` types, `camelBack` functions, private members with `_` suffix,
  constants `kName`. clang-tidy warnings are errors.
- Avoid `<format>` in firmware code (pulls in ~300 KB of libstdc++ locale code); use `snprintf`.

**Ownership and lifetime**
- No mutable globals. `App` is the composition root and owns every object as a member, in dependency order
  (e.g. routes before `WebServer`, so the server stops first); `app_main()` only runs a static `App`.
- Dependencies are passed by reference in the constructor, never looked up globally.
- RAII for every IDF handle (I2C bus/device, LEDC channel, `esp_timer`, httpd, mounted FS, open file, OTA
  handle, task). Wrappers are non-copyable, movable only where it helps.
- No raw `new`/`delete`; `std::unique_ptr` where heap is needed. The control path is allocation-free.

**Errors and types**
- No exceptions, no RTTI: fallible operations return `banana::Result<T>` (`std::expected<T, esp_err_t>`;
  `fail(ESP_ERR_...)`, `toResult(err)`), marked `[[nodiscard]]`. `ESP_ERROR_CHECK` only in `App` for fatal
  boot errors.
- Constructors cannot fail: objects that need hardware init use a static factory
  (`static Result<Ads1115> create(...)`).
- `enum class` for states and errors, `Flags<E>` for bit sets, pins as `constexpr` in `banana::board`,
  `std::string_view` / `std::span` / `std::chrono` durations instead of raw pointers and integer times.

**Testability**
- `banana_control` and other pure logic see hardware only through the `io` interfaces (`ITemperatureSensor`,
  `IPwmOutput`, ...), so the same code runs on the ESP32 and on the host with fakes. Keep such sources free
  of ESP-IDF headers; add new ones to `test/host/CMakeLists.txt` together with tests.
- Variants are strategies, not `#ifdef`s (e.g. `ITemperatureConverter`: linear, quadratic, lookup table).

**RTOS and callbacks**
- Tasks derive from `rtos::Task` and override `run()`. Every C callback (ISR, `esp_timer`, event handler,
  httpd handler, log hook) goes through a static member trampoline with `this` as `void*`.
- ISRs are `IRAM_ATTR` and only set an event bit or notify a task: no I2C, logging, heap or virtual calls.
- Shared state: `std::mutex` + `std::scoped_lock`; `portMUX` spinlocks only for data shared with an ISR.
  The concurrency rules per task are in docs/ARCHITECTURE.md.

## Things to know

- **Web UI works offline** (SoftAP mode, no internet): no CDNs, all assets live in `data/`. Every new
  file there must also be added to `kStaticFiles` in `components/banana_web/include/banana/web/WebPaths.hpp`,
  otherwise it is not served. Keep pages small (LittleFS, slow ESP32 web server).
- `data/ota.html` is also gzip-embedded into the firmware as the `/failsafe` page, so it must work without
  LittleFS (no dependency on other files for its function).
- JSON keys of `/lastvalues.json`, `params.json` and the `data.csv` columns come from the Arduino firmware;
  do not rename them.
- Live data reaches the pages through server-sent events on `/events` (`values` once per second, `rows`
  with new `data.csv` rows); pages fall back to polling.
- `WebServer` allows 16 URI handlers (`maxUriHandlers`); check the count when adding routes.
- **Versioning:** no version number in the sources. `CMakeLists.txt` derives it from git tags `vX.Y.Z`
  (`git describe`), exposed via `esp_app_get_description()->version` and `/version.json`. Releases:
  update `CHANGELOG.md`, tag `vX.Y.Z`, push the tag (see README "Versioning and releases").
- OTA images must confirm themselves (`OtaUpdater::markRunningAppValid()` once the web server is up),
  otherwise the bootloader rolls back.
