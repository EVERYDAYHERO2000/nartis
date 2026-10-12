"""The Nartis RF Meter integration."""

from __future__ import annotations

from homeassistant.const import CONF_HOST, Platform
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .api import NartisApi
from .coordinator import NartisConfigEntry, NartisCoordinator

PLATFORMS: list[Platform] = [Platform.SENSOR, Platform.BINARY_SENSOR]


async def async_setup_entry(hass: HomeAssistant, entry: NartisConfigEntry) -> bool:
    """Set up a Nartis device from a config entry."""
    api = NartisApi(async_get_clientsession(hass), entry.data[CONF_HOST])
    coordinator = NartisCoordinator(hass, entry, api)
    await coordinator.async_config_entry_first_refresh()
    entry.runtime_data = coordinator

    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    entry.async_on_unload(entry.add_update_listener(_async_update_listener))
    return True


async def async_unload_entry(hass: HomeAssistant, entry: NartisConfigEntry) -> bool:
    """Unload a config entry."""
    return await hass.config_entries.async_unload_platforms(entry, PLATFORMS)


async def _async_update_listener(hass: HomeAssistant, entry: NartisConfigEntry) -> None:
    """Reload when options (scan interval) change."""
    await hass.config_entries.async_reload(entry.entry_id)
