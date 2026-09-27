# Architecture

## Components

One ESP-IDF component per layer. Arrows point to what a component uses. Components marked *host* are
pure logic and run in the host unit tests (`test/host`).

```mermaid
flowchart TD
    main["<b>main</b><br/>App (composition root)<br/>HeaterTask"]

    web["<b>banana_web</b><br/>WebServer, HttpRequest<br/>ApiRoutes, StaticFileRoutes, OtaRoutes<br/>OtaUpdater · WebPaths, LiveValues (host)"]
    net["<b>banana_net</b><br/>WifiManager, MdnsService, TimeSync"]
    storage["<b>banana_storage</b><br/>LittleFs, Nvs, File<br/>FileLogger, MeasurementRecorder<br/>MeasurementCsv (host)"]
    config["<b>banana_config</b><br/>Config (host), ConfigJson (host)<br/>ConfigStore"]
    control["<b>banana_control</b> (host)<br/>ControlLoop, PidController<br/>BrewFeedForward, BrewDetector<br/>Diagnostics, StatusIndicator<br/>IHeaterControl"]
    drivers["<b>banana_drivers</b><br/>Ads1115, Ssr, RgbLed<br/>TemperatureConverter, Pt1000 (host)"]
    hal["<b>banana_hal</b><br/>GpioInput/Output, I2cBus/Device, Ledc<br/>PeriodicTimer · rtos: Task, EventGroup, Watchdog"]
    core["<b>banana_core</b> (host)<br/>Result, Flags, board pins<br/>io interfaces: ITemperatureSensor,<br/>IPwmOutput, IStatusLed, IDigitalInput, INetworkStatus"]
    idf[["ESP-IDF v6.1<br/>drivers, FreeRTOS, esp_http_server, esp_wifi, lwIP,<br/>app_update · managed: littlefs, mdns, ArduinoJson"]]

    main --> web & net & storage & config & control & drivers & hal
    web --> net & config & control & storage
    net --> hal
    storage --> hal
    config --> storage
    control --> config & core
    drivers --> hal & config
    hal --> core
    web & net & storage & drivers & hal --> idf
```

The control logic only sees the `io` interfaces (`ITemperatureSensor`, `IPwmOutput`, …), so the same
`ControlLoop` runs on the ESP32 with the real drivers and on the host with fakes. The web routes see the
heater only through `control::IHeaterControl` (`snapshot()`, `requestConfig()`).

## Runtime: tasks, interrupts, data flow

13 FreeRTOS tasks run in steady state (measured with `uxTaskGetSystemState()`; `main` ends after
`App::run()`). Two of them are ours (**bold**); the rest belong to ESP-IDF.

```mermaid
flowchart LR
    subgraph HW["Hardware"]
        ads["ADS1115<br/>Pt1000, 8 SPS"]
        pump["Pump relay<br/>GPIO 17"]
        ssr["SSR<br/>GPIO 21"]
        led["RGB LED"]
        flash[("Flash<br/>LittleFS /fs · OTA slots")]
    end

    subgraph ISR["Interrupts"]
        rdyIsr["ALERT/RDY ISR<br/>GPIO 14"]
        pumpIsr["Pump edge ISR"]
    end

    subgraph Tasks["FreeRTOS tasks"]
        tmr["Tmr Svc<br/>prio 1"]
        esptimer["esp_timer<br/>prio 22"]
        heater["<b>heater</b><br/>prio 5"]
        httpd["httpd<br/>prio 5"]
        filelog["<b>filelog</b><br/>prio 2"]
        net["wifi 23 · tcpip 18<br/>sys_evt 20 · mdns 1"]
    end

    ev{{"EventGroup<br/>SampleReady · Tick<br/>PumpEdge · ConfigChanged"}}
    snap[("ProcessSnapshot<br/>+ pending Config<br/>(mutex)")]
    ring[("Ring buffer 8 kB<br/>all log output")]

    ads -- "conversion ready" --> rdyIsr
    pump --> pumpIsr
    rdyIsr -- "setFromIsr" --> tmr
    pumpIsr -- "setFromIsr" --> tmr
    tmr --> ev
    esptimer -- "450 ms tick" --> ev
    esptimer -. "60 s health log" .-> ring
    ev --> heater

    heater -- "I2C read" --> ads
    heater -- "LEDC duty" --> ssr
    heater -- "LEDC colour" --> led
    heater -- "data.csv, sync every 10 rows" --> flash
    heater <--> snap
    heater -. "log" .-> ring

    httpd -- "snapshot() /lastvalues.json" --> snap
    httpd -- "requestConfig()" --> snap
    httpd -- "ConfigChanged" --> ev
    httpd -- "params.json, uploads, OTA image,<br/>read pages/data.csv/logs" --> flash
    net <--> httpd
    net -. "INetworkStatus<br/>(Wi-Fi fault = purple LED)" .-> heater

    ring --> filelog
    filelog -- "logfile_recent.txt" --> flash
```

### Task list

| Task        | Prio | Core        | Stack free* | Owner / purpose                                                        |
|-------------|------|-------------|-------------|------------------------------------------------------------------------|
| **heater**  | 5    | 1 (pinned)  | 1756 B / 4096 | `HeaterTask`: ADC read, PID, SSR, LED, brew detection, diagnostics, `data.csv`; subscribed to the task watchdog (75 s) |
| **filelog** | 2    | 1 (pinned)  | 1800 B / 4096 | `FileLogger`: ring buffer → `logfile_recent.txt`, rotation            |
| httpd       | 5    | any         | 7268 B / 8192 | `esp_http_server`: all web routes, OTA and file uploads               |
| esp_timer   | 22   | 0           | 2816 B      | 450 ms tick (`PeriodicTimer`), 60 s health log                         |
| Tmr Svc     | 1    | any         | 1472 B      | FreeRTOS timer daemon: carries `EventGroup::setFromIsr()` to the event group |
| sys_evt     | 20   | 0           | 1492 B      | default event loop: `WifiManager::onEvent()` (connect, reconnect, got IP) |
| wifi        | 23   | 0           | 3232 B      | Wi-Fi driver                                                           |
| tcpip       | 18   | any         | 2200 B      | lwIP stack, SNTP                                                       |
| mdns        | 1    | 0           | 2192 B      | `coffee.local` responder                                               |
| ipc0, ipc1  | 24   | 0, 1        | 412 / 532 B | inter-core calls (e.g. stalling the other core during flash writes)     |
| IDLE0, IDLE1| 0    | 0, 1        | ~980 B      | idle tasks                                                             |

\* minimum free stack since boot (high-water mark), 60 s after boot in SoftAP mode.

`heater` and `filelog` are pinned to core 1 (`kControlCore` in `App.cpp`); the Wi-Fi tasks run on core 0.
Sharing the core makes the two take turns (the heater has the higher priority), so a log write to flash
rarely overlaps an ADS1115 read. Bench, stress build with one log line per tick, 170 s: 16 I2C NACKs with
`filelog` unpinned, 23 on core 0, 0–1 on core 1. A flash write still pauses *both* cores on the ESP32, so the
pinning does not shorten the stalls of the heater task, it only avoids the overlap.

### Concurrency rules

- The heater task is the only task that touches the ADC, SSR and LED; everything else talks to it through the
  event group (`requestConfig()` + `ConfigChanged`) or reads the mutex-protected `ProcessSnapshot`.
- ISRs only set event bits (IRAM), no I2C or logging.
- `ConfigStore` has its own mutex (web saves vs. boot load); `params.json` is written as a temp file + rename.
- Every task logs through the `vprintf` hook into the ring buffer; only `filelog` writes the log file, so a
  slow flash write never blocks the logging task (full buffer → dropped lines, counted).
- Flash writes from a task on the other core (httpd: settings, uploads, OTA) can still overlap an ADS1115
  read; the driver retries the read once (see MIGRATION.md, Phase 7 bench finding).
