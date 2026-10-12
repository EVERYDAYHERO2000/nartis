"""Base entity for the Nartis RF Meter integration."""

from __future__ import annotations

from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN, MANUFACTURER, MODEL
from .coordinator import NartisCoordinator


class NartisEntity(CoordinatorEntity[NartisCoordinator]):
    """Entity bound to the single Nartis device of a config entry."""

    _attr_has_entity_name = True

    def __init__(self, coordinator: NartisCoordinator, key: str) -> None:
        super().__init__(coordinator)
        entry = coordinator.config_entry
        device_id = entry.unique_id or entry.entry_id
        self._attr_unique_id = f"{device_id}_{key}"
        self._attr_device_info = DeviceInfo(
            identifiers={(DOMAIN, device_id)},
            name=entry.title,
            manufacturer=MANUFACTURER,
            model=MODEL,
            serial_number=coordinator.data.serial or None,
            configuration_url=coordinator.api.base_url,
        )
