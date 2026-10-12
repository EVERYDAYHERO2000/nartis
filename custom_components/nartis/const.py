"""Constants for the Nartis RF Meter integration."""

from __future__ import annotations

DOMAIN = "nartis"

CONF_SCAN_INTERVAL = "scan_interval"
DEFAULT_SCAN_INTERVAL = 60  # seconds; the device itself polls the meter every few minutes
MIN_SCAN_INTERVAL = 10
MAX_SCAN_INTERVAL = 3600

REQUEST_TIMEOUT = 10  # seconds

MANUFACTURER = "Нартис"
MODEL = "И100 / И300 (RF433)"
