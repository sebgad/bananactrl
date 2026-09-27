# bananactrl

Temperature control for a Rancilio Silvia espresso machine on a SparkFun ESP32 Thing Plus: Pt1000 via
ADS1115, PID-controlled SSR, RGB status LED, web UI with graphs, settings and OTA.
ESP-IDF v6.1 port of the Arduino firmware `coffee_ctrl_main`; see [MIGRATION.md](MIGRATION.md) for the design
and the differences, [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for components, tasks and data flow.

## Build

```sh
. /opt/esp/esp-idf-v6.1/export.sh
idf.py build
```

Factory Wi-Fi credentials (used while the settings contain no SSID) go into the git-ignored
`sdkconfig.defaults.local`:

```
CONFIG_BANANA_WIFI_FACTORY_SSID="..."
CONFIG_BANANA_WIFI_FACTORY_PW="..."
```

`sdkconfig` is generated from `sdkconfig.defaults` (+ `.local`). After changing a defaults file, delete
`sdkconfig` and build again.

## Flash

| Command                                | Writes                                                            | Settings (NVS) | data.csv, logs |
|----------------------------------------|-------------------------------------------------------------------|----------------|----------------|
| `idf.py -p /dev/ttyUSB0 flash`         | bootloader, partition table, OTA data, app, LittleFS image (`data/`) | kept        | **erased**     |
| `idf.py -p /dev/ttyUSB0 app-flash`     | app only                                                          | kept           | kept           |
| `idf.py -p /dev/ttyUSB0 storage-flash` | LittleFS image only (web pages)                                   | kept           | **erased**     |
| `idf.py -p /dev/ttyUSB0 erase-flash`   | everything                                                        | **erased**     | **erased**     |
| Web UI → OTA → firmware                | app into the other OTA slot (MD5: `md5sum build/bananactrl.bin`)  | kept           | kept           |
| Web UI → OTA → data file               | one file into LittleFS (e.g. an updated `index.html`)             | kept           | kept           |

The settings live in NVS (namespace `banana`, same JSON as `params.json`). A `params.json` found in LittleFS
while NVS holds no settings is imported once and renamed to `params.json.imported`.

After an OTA update the new firmware confirms itself once its web server runs; if it resets before that, the
bootloader returns to the previous firmware. `/failsafe` serves an upload page built into the firmware
(works without LittleFS).

## Cut-over from the Arduino firmware

The partition table differs, so the first installation has to go over USB, and it erases the old
file system (the Arduino `params.json` is lost with it, hence the download).

1. Old firmware: Settings → **Download config** (`silvia_ctrl_config.json`). Optionally download
   `data.csv` from Graphs.
2. `idf.py -p /dev/ttyUSB0 flash` (full flash, see above).
3. The ESP joins the factory Wi-Fi from `sdkconfig.defaults.local` (http://coffee.local/); if that fails
   within 18 s it opens the SoftAP `SilviaCoffeeCtrl` (open, http://192.168.4.1/).
4. Settings → **Upload config** with the downloaded file. The key names are unchanged, so the old file is
   accepted as is. Wi-Fi changes take effect after **Restart**.

The settings page never shows the stored Wi-Fi password; leaving the field empty keeps it.

## Web UI

| URL                  | Content                                                   |
|----------------------|-----------------------------------------------------------|
| `/`                  | live gauges (SVG, pushed via `/events`)                   |
| `/graphs.html`       | `data.csv` of the current session, live rows via `/events` (uPlot, bundled: no internet needed) |
| `/events`            | server-sent events once per second: `values` (as `/lastvalues.json`), `rows` (new `data.csv` rows) |
| `/settings.html`     | parameters (`/params.json`, `/paramUpdate`, `/paramReset`, `/restartesp`) |
| `/ota.html`          | firmware and data file upload                             |
| `/log.html`          | `logfile_recent.txt` (this session), `logfile_last.txt` (previous session) |

## Home Assistant (MQTT, optional)

Settings → section **MQTT**: `MqttEnabled`, `MqttHost` (e.g. the Home Assistant host with the Mosquitto add-on),
`MqttPort` (1883), `MqttUser`, `MqttPassword` (never shown; empty keeps the stored one). Save, then **Restart**.
MQTT only runs when the ESP is connected to a Wi-Fi network (not in SoftAP mode).

The device appears in Home Assistant via MQTT discovery as **BananaCoffee** with:

| Entity                   | Type          | Notes                                                        |
|--------------------------|---------------|--------------------------------------------------------------|
| Temperature              | sensor        | °C                                                           |
| Heater power             | sensor        | %                                                            |
| Time to standby          | sensor        | min until standby (counted from boot, as before)            |
| Brewing, Standby         | binary_sensor |                                                              |
| Fault, Fault details     | binary_sensor, sensor | diagnostic                                           |
| Wi-Fi signal             | sensor        | %, diagnostic                                                |
| Target temperature       | number        | 60–100 °C, step 0.5 (limits enforced on the device)          |
| Standby after            | number        | 10–600 min; below the uptime = standby now                   |
| Restart                  | button        | also leaves standby                                          |

Changes from Home Assistant are saved like changes on the settings page. State is published every 5 s;
availability via `bananactrl/<id>/availability` (last will `offline`).

## Tests and checks

```sh
# host unit tests (needs `idf.py build` once for the managed ArduinoJson)
cmake -S test/host -B build-host -G Ninja && cmake --build build-host && ctest --test-dir build-host

# clang-tidy with Espressif's clang (python $IDF_PATH/tools/idf_tools.py install esp-clang)
tools/clang_tidy.py
```

Once a minute the log shows `health: heap free …, min …; log dropped …, write errors …` for soak tests.
