"""Data update coordinator for the Nartis RF Meter integration."""

from __future__ import annotations

from datetime import datetime, timedelta
import logging

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed
from homeassistant.util import dt as dt_util

from .api import NartisApi, NartisApiError, NartisData
from .const import CONF_SCAN_INTERVAL, DEFAULT_SCAN_INTERVAL, DOMAIN

_LOGGER = logging.getLogger(__name__)

type NartisConfigEntry = ConfigEntry[NartisCoordinator]


class NartisCoordinator(DataUpdateCoordinator[NartisData]):
    """Polls the device's /api/readings and /api/status."""

    config_entry: NartisConfigEntry

    def __init__(self, hass: HomeAssistant, entry: NartisConfigEntry, api: NartisApi) -> None:
        interval = entry.options.get(CONF_SCAN_INTERVAL, DEFAULT_SCAN_INTERVAL)
        super().__init__(
            hass,
            _LOGGER,
            config_entry=entry,
            name=DOMAIN,
            update_interval=timedelta(seconds=interval),
        )
        self.api = api
        self.last_reading: datetime | None = None

    async def _async_update_data(self) -> NartisData:
        try:
            data = await self.api.async_get_data()
        except NartisApiError as err:
            raise UpdateFailed(f"Cannot reach Nartis device at {self.api.base_url}: {err}") from err

        # The device reports how old its readings are; convert that to a stable
        # timestamp (ignore the few seconds of jitter between polls).
        if data.age is not None:
            reading = (dt_util.utcnow() - timedelta(seconds=data.age)).replace(microsecond=0)
            if self.last_reading is None or abs((reading - self.last_reading).total_seconds()) > 5:
                self.last_reading = reading
        return data
