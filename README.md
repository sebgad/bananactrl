# bananactrl

Temperature control for a Rancilio Silvia espresso machine on a SparkFun ESP32 Thing Plus: Pt1000 via
ADS1115, PID-controlled SSR, RGB status LED, web UI with graphs, settings and OTA.
ESP-IDF v6.1 port of the Arduino firmware `coffee_ctrl_main`; see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
for components, tasks, data flow and the differences from the Arduino firmware.

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
| Web UI → OTA → Update from GitHub      | newest release: app into the other OTA slot, web pages into LittleFS | kept        | kept           |
| Web UI → OTA → Firmware upload         | files from `build/dist/`: web pages into LittleFS (optional), app into the other OTA slot | kept | kept |

The settings live in NVS (namespace `banana`, same JSON as `params.json`). A `params.json` found in LittleFS
while NVS holds no settings is imported once and renamed to `params.json.imported`.

### Update from GitHub

OTA page → **Check for updates** asks the GitHub API for the newest release of
`CONFIG_BANANA_UPDATE_REPOSITORY` (default `sebgad/bananactrl`; tick **Include pre-releases** for release
candidates). **Install** makes the controller download it itself (station mode only, the SoftAP has no
internet):

1. `MD5SUMS` of the release,
2. `webui-<version>.tar` into hidden files in LittleFS (releases without the archive keep their pages),
3. `bananactrl-<version>.bin` into the other OTA slot,
4. after both MD5s and the image check passed: the pages are renamed into place and the controller restarts.

A failed download leaves firmware and pages as they were. The heater keeps running during the download
(about a minute). `params.json`, `data.csv` and the logs are never part of the archive and stay untouched.
The API allows 60 unauthenticated requests per hour and IP address; each check uses one.

### Upload a local build

Every `idf.py build` also writes the release assets (the same files as a GitHub release, see
[Release assets](#making-a-release)) to `build/dist/` ([tools/dist.py](tools/dist.py)). On the OTA page,
**Firmware upload** takes one file per field:

| Field               | File from `build/dist/`    |
|---------------------|----------------------------|
| Firmware            | `bananactrl-<version>.bin` |
| Checksums           | `MD5SUMS`                  |
| Web UI (optional)   | `webui-<version>.tar`      |

The browser unpacks the archive and uploads its files one by one, then the firmware (checked against its MD5
from `MD5SUMS`), shows the progress over all files and reloads the page once the controller has restarted.
Without an archive only the firmware is uploaded. Unlike the update from GitHub, the pages are replaced before the firmware is written: if the
firmware upload fails, the new pages run with the old firmware until the upload is repeated.

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
| `/ota.html`          | update from GitHub releases (`/update.json`, `/update`), firmware upload with optional web UI archive |
| `/log.html`          | `logfile_recent.txt` (this session), `logfile_last.txt` (previous session) |

## Status LED

| Colour           | Meaning                                                  |
|------------------|----------------------------------------------------------|
| white            | booting                                                  |
| orange, pulsing  | heating up (more than `ReadyBand`, 1 K, below the target) |
| green            | ready (within ±`ReadyBand` of the target)                |
| blue, pulsing    | cooling down (more than `ReadyBand` above the target)    |
| red              | brewing                                                  |
| magenta, blinking | steam mode: heating up to the bimetal temperature       |
| magenta          | steam mode: ready (119 °C reached)                       |
| purple           | fault (heater off, except for a Wi-Fi fault)             |

Pulsing = fading in and out once per 2 s, blinking = on/off once per second. Brightness per colour: settings
section `LED` (magenta, Telekom #E20074, has no factor of its own; only the channel factors apply).

### Steam mode

There is no input for the steam switch: it bypasses the SSR and heats the boiler up to the bimetal switch
(about 120 °C), which the PID never does. So the temperature alone decides:

| Temperature                         | State                                              |
|-------------------------------------|----------------------------------------------------|
| rises to 105 °C                     | steam mode, heating up (magenta blinking)          |
| reaches 119 °C                      | steam ready (magenta)                              |
| falls below 115 °C                  | steam mode over: cooling down to the target (blue) |
| reaches 119 °C again (bimetal)      | steam ready again                                  |
| falls below 100 °C                  | steam detection armed again                        |

In steam mode the SSR stays off and the PID is frozen; it starts clean afterwards.

The temperatures are settings (section **Steam**, defaults as in the table above):

| Key                      | Default | Meaning                                                           |
|--------------------------|---------|-------------------------------------------------------------------|
| `SteamDetectionActivate` | true    | steam detection on/off                                            |
| `SteamEnterTemp`         | 105     | rising through it starts steam mode                               |
| `SteamExitTemp`          | 100     | below it steam detection is armed again                           |
| `SteamReadyTemp`         | 119     | steam ready                                                       |
| `SteamReadyLeaveTemp`    | 115     | below it steam mode ends                                          |

They must satisfy `SteamExitTemp < SteamEnterTemp < SteamReadyTemp` and
`SteamExitTemp < SteamReadyLeaveTemp < SteamReadyTemp`; otherwise the update is rejected. Steam detection
is off while the target is not below `SteamExitTemp` (logged as a warning). `PID` →
`ReadyBand` (default 1 K) is the band around the target that counts as ready (green).

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
| State                    | sensor (enum) | `heating_up`, `ready`, `cooling_down`, `brewing`, `steam_heating_up`, `steam_ready`, `fault` |
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

## Versioning and releases

Versions follow [Semantic Versioning](https://semver.org/) and come from git tags `vMAJOR.MINOR.PATCH`; there is
no version number in the sources. `CMakeLists.txt` runs `git describe` and embeds the result in the app
descriptor (`esp_app_get_description()->version`). It is shown in the web UI footer, served as `/version.json`,
reported to Home Assistant and printed at boot (`App version:`).

| Checkout                          | Firmware version     |
|-----------------------------------|----------------------|
| on tag `v1.2.0`                   | `1.2.0`              |
| 3 commits after it                | `1.2.0-3-gabc1234`   |
| with uncommitted changes          | `…-dirty`            |
| no tag yet                        | `0.0.0-gabc1234`     |

Set `BANANA_VERSION` in the environment to override it (e.g. for a build from a source archive without `.git`).

- **MAJOR**: incompatible changes: the settings/NVS layout or `params.json` keys change without migration,
  the partition table changes (needs a USB flash, OTA is not enough), MQTT topics or HTTP routes are removed.
- **MINOR**: new features, compatible with existing settings and the partition table.
- **PATCH**: bug fixes only.
- Pre-releases: `v1.3.0-rc.1`.

The web pages in `data/` are part of the same version: the LittleFS image (`storage.bin`) is built from the
tagged tree too.

### Making a release

1. Move the entries under `## [Unreleased]` in [CHANGELOG.md](CHANGELOG.md) to a new `## [1.2.0] - YYYY-MM-DD`
   section and commit it on `main`.
2. Tag and push:
   ```sh
   git tag -a v1.2.0 -m "bananactrl 1.2.0"
   git push origin main v1.2.0
   ```
3. [.github/workflows/release.yml](.github/workflows/release.yml) builds the tag, checks that the firmware
   reports exactly `1.2.0`, and creates the GitHub release with the changelog section as notes. Tags with a
   `-` (`v1.3.0-rc.1`) become pre-releases.

Release assets (a local build writes the same files to `build/dist/`):

| File                             | Use                                                                    |
|----------------------------------|------------------------------------------------------------------------|
| `bananactrl-<version>.bin`       | OTA page → Firmware upload (MD5 from `MD5SUMS`) or `app-flash`         |
| `webui-<version>.tar`            | the files of `data/`: installed with the firmware (Update from GitHub, Firmware upload) |
| `storage-<version>.bin`          | LittleFS image (web pages); erases data.csv and logs                   |
| `bananactrl-factory-<version>.bin` | everything merged, for a new board: `esptool.py write_flash 0x0 bananactrl-factory-<version>.bin` |
| `bootloader.bin`, `partition-table.bin`, `ota_data_initial.bin`, `flash_args` | individual images and their offsets |

[.github/workflows/build.yml](.github/workflows/build.yml) builds the firmware and runs the host tests for
every push to `main` and every pull request.
