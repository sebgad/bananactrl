# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versions follow
[Semantic Versioning](https://semver.org/) (see "Versioning and releases" in [README.md](README.md)).

## [Unreleased]

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

[Unreleased]: https://github.com/sebgad/bananactrl/compare/v1.0.0-rc.3...HEAD
[1.0.0-rc.3]: https://github.com/sebgad/bananactrl/compare/v1.0.0-rc.2...v1.0.0-rc.3
[1.0.0-rc.2]: https://github.com/sebgad/bananactrl/compare/v1.0.0-rc.1...v1.0.0-rc.2
[1.0.0-rc.1]: https://github.com/sebgad/bananactrl/releases/tag/v1.0.0-rc.1
