# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versions follow
[Semantic Versioning](https://semver.org/) (see "Versioning and releases" in [README.md](README.md)).

## [Unreleased]

## [1.0.0-rc.6] - 2026-10-02

### Added
- [docs/FEATURES.md](docs/FEATURES.md): feature overview with screenshots of the web UI.
- [docs/CALIBRATION.md](docs/CALIBRATION.md): every setting explained, with charts (simulations with the
  firmware's controller on a boiler model fitted to measurements, and a measured steam session).

### Fixed
- Graphs page: the temperature axis grows beyond 100 °C, so steam mode (up to about 125 °C) is no longer
  cut off.

## [1.0.0-rc.5] - 2026-10-02

### Added
- Steam mode, detected from the temperature (no input for the steam switch): from 105 °C the LED blinks
  magenta (Telekom #E20074), from 119 °C it is steady magenta; below 115 °C steam mode ends and the LED shows
  cooling down (blue). The SSR stays off and the PID is frozen while steaming.
- Settings section `Steam` (detection on/off, the steam temperatures) and `PID` → `ReadyBand` (the ±1 K
  around the target that counts as ready). `/paramUpdate` rejects thresholds in the wrong order. Stored
  settings without these keys get the defaults.
- Machine state (`heating_up`, `ready`, `cooling_down`, `brewing`, `steam_heating_up`, `steam_ready`,
  `fault`) as Home Assistant enum sensor "State", as `State` in `/lastvalues.json` and in the log.

## [1.0.0-rc.4] - 2026-09-29

### Added
- Every build writes the release assets (`bananactrl-<version>.bin`, `webui-<version>.tar`, `MD5SUMS`, ...)
  to `build/dist/`, the same files as a GitHub release.
- OTA page: **Firmware upload** with one field each for the firmware, `MD5SUMS` and the (optional) web UI
  archive, and a progress bar; the MD5 no longer has to be typed in.

### Removed
- OTA page: upload of single web UI files (the web UI archive replaces it).

## [1.0.0-rc.3] - 2026-09-28

### Changed
- Reworked web UI: muted banana yellow on espresso brown, sticky top bar, cards, automatic dark mode (also
  for the charts). Settings sections are shown as cards; menu entries "OTA" and "Debug Log" are now
  "Update" and "Log".

### Fixed
- The debug log is shown as plain text (no HTML from log lines) and loads immediately.

## [1.0.0-rc.2] - 2026-09-28

### Added
- Update from GitHub releases on the OTA page: the controller checks for the newest release (optionally
  pre-releases) and installs firmware and web pages itself, verified against `MD5SUMS`.
- Release asset `webui-<version>.tar` with the web pages, for these updates.

## [1.0.0-rc.1] - 2026-09-28

### Added
- Firmware version from git tags, shown in the web UI footer and served as `/version.json`.
- GitHub Actions: build and host tests for every push and pull request, releases from `v*` tags.
- Dashboard widgets with uPlot trends for temperature, heater, PID terms and Wi-Fi signal.
- Status LED pulses while heating up or cooling down.
- Optional MQTT service with Home Assistant discovery.
- Bundled uPlot charts, server-sent events instead of polling, settings stored in NVS.

### Changed
- The PID integrator is kept across settings changes that do not affect it.
- Heater control and log writer run pinned to core 1.

### Initial port
- ESP-IDF v6.1 / C++ port of the Arduino firmware `coffee_ctrl_main`: Pt1000 via ADS1115, PID-controlled
  SSR, web UI with graphs, settings and OTA, measurement recording and logging (differences from the Arduino
  firmware: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#differences-from-the-arduino-firmware)).

[Unreleased]: https://github.com/sebgad/bananactrl/compare/v1.0.0-rc.6...HEAD
[1.0.0-rc.6]: https://github.com/sebgad/bananactrl/compare/v1.0.0-rc.5...v1.0.0-rc.6
[1.0.0-rc.5]: https://github.com/sebgad/bananactrl/compare/v1.0.0-rc.4...v1.0.0-rc.5
[1.0.0-rc.4]: https://github.com/sebgad/bananactrl/compare/v1.0.0-rc.3...v1.0.0-rc.4
[1.0.0-rc.3]: https://github.com/sebgad/bananactrl/compare/v1.0.0-rc.2...v1.0.0-rc.3
[1.0.0-rc.2]: https://github.com/sebgad/bananactrl/compare/v1.0.0-rc.1...v1.0.0-rc.2
[1.0.0-rc.1]: https://github.com/sebgad/bananactrl/releases/tag/v1.0.0-rc.1
