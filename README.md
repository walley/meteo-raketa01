# meteo-raketa01

Meteo-raketa01 is a small research / hobby project around "raketa" — a shaped IoT device that collects and displays room-environment data. The project contains firmware, data-logging tools, and analysis/visualization utilities for temperature, humidity, pressure and other indoor environmental telemetry.

## Project status
- Status: Draft / In development
- Target hardware: microcontroller-based device (common boards such as ESP32, STM32, etc.), environmental sensors (temperature, humidity, pressure), optional light, motion, and air-quality sensors, and local storage or network uplink.
- Software stack: embedded firmware for data acquisition, device-side telemetry/hosting, and a set of analysis & visualization tools. The README intentionally stays generic about specific languages or runtimes to keep the project portable.

## Goals
- Continuously monitor indoor environment metrics with a small, low-power device.
- Provide reliable local logging and optional network streaming to a dashboard.
- Offer tools to parse, visualize, and export logs for further inspection.
- Make device maintenance, calibration, and OTA updates straightforward.

## Features
- Sensor sampling (temperature, humidity, pressure, optional VOC/CO2, light, motion)
- Timestamped telemetry with consistent timebase (ISO 8601 / UNIX epoch)
- Local storage (SD or flash) with rotation and integrity checks
- Network reporting over HTTP/MQTT/CoAP or simple TCP/UDP for dashboards
- Simple web UI served by the device for real-time viewing and configuration
- Data export to common formats (JSON, CSV, KML) for downstream tools
- Calibration and diagnostics utilities to validate sensors and battery

## Repository layout (suggested)
- /firmware/ — microcontroller projects (PlatformIO, Arduino, Zephyr, etc.)
- /device-web/ — web UI and static assets served by the device
- /logger/ — tools for parsing and converting raw telemetry files
- /analysis/ — scripts and notebooks for plotting & processing telemetry
- /docs/ — wiring diagrams, BOM, datasheets, and user guides
- /tests/ — test data and validation harnesses
- README.md — this file

## Getting started (development)
Requirements
- Toolchains for the chosen firmware framework (PlatformIO, Arduino CLI, or the SDK for the chosen MCU)
- Optional: container environment (Docker) for reproducible builds and CI

Build & flash (example)
```bash
# build with PlatformIO (example)
cd firmware
pio run -e <env-name>
# to upload
pio run -e <env-name> -t upload
```

Device configuration
- Configure Wi‑Fi, time source (NTP), and telemetry target via the web UI or a local config file.
- Secure the device with a password and, if exposing services on a network, consider using TLS or a secure tunnel.

## Telemetry format
Recommended minimal JSON-per-line telemetry record (one JSON object per line):
{
  "ts": "2026-02-05T12:34:56.789Z",
  "epoch": 176\n,
  "sensors": {
    "temp_c": 22.5,
    "humidity_pct": 45.2,
    "pressure_pa": 101325,
    "light_lux": 120,
    "voc": 0.45
  },
  "device": {
    "id": "raketa-01",
    "rssi": -62,
    "vbat": 3.75
  },
  "event": "motion_detected"  // optional event markers
}

CSV is supported for compatibility — include header with ISO timestamps.

## Data processing & visualization
- Quick plotting utilities for trends (temperature, humidity, pressure).
- Dashboard examples: simple device-hosted web UI and server-side ingestion with map/graph visualizations.
- Export tools: convert telemetry to CSV, KML for mapping, or other exchange formats for analytics.

Suggested visualizations:
- Temperature, humidity, and pressure vs time
- Multi-sensor overlays for correlation (e.g., temp vs humidity)
- Indoor heatmaps (if multiple devices are deployed)
- Battery voltage and connectivity health over time

## Telemetry transport & storage
- Local storage: line-buffered log files with periodic rotation (e.g., daily) and checksum.
- Network streaming: small JSON records over MQTT/HTTP/CoAP; consider compact binary framing where bandwidth is constrained.
- Message design: include a sequence number and a CRC for loss detection and reassembly.

## Calibration & testing
- Sensor calibration routines (offset & scale) and test modes to capture raw readings.
- Stationary tests to estimate sensor bias and noise.
- Power tests to measure battery life under typical sampling and reporting intervals.
- Integration tests for network reconnection and logging under intermittent connectivity.

## Security & deployment
- Secure onboarding: protect initial configuration and password setting.
- Over-the-air update mechanism with signed images where possible.
- Network exposure: minimize publicly accessible endpoints; use network-level controls or VPNs.
- Data privacy: avoid sending personally-identifying data; document telemetry retention and export policies.

## Contributing
- Open issues for bugs, feature requests, and hardware suggestions.
- Follow the style and tooling used in each subproject (linters, formatters).
- Add tests for telemetry parsing and device behaviors.
- Contribute wiring diagrams and validated parts lists to /docs/.

## Example commands
- Build firmware (PlatformIO):
```bash
cd firmware
pio run -e esp32dev -t upload
```
- Convert a raw log file to CSV:
```bash
./logger/convert_log --input logs/session.log --output exports/session.csv
```

## License
Choose and add a license file (for example: MIT). Add LICENSE at repository root.

## Contacts
- Maintainer: walley
- For questions: open an issue or start a discussion in the repository.

## TODO
- Add detailed wiring diagrams and images of the assembled device
- Provide a binary telemetry spec (compact frame) for low-bandwidth links
- Add CI to validate parsing tools and sample datasets
- Provide a set of example dashboards and a small sample dataset with annotated events
