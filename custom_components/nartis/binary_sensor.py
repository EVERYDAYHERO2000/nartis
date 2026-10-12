"""Binary sensors for the Nartis RF Meter integration."""

from __future__ import annotations

from homeassistant.components.binary_sensor import (
    BinarySensorDeviceClass,
    BinarySensorEntity,
    BinarySensorEntityDescription,
)
from homeassistant.const import EntityCategory
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddConfigEntryEntitiesCallback

from .coordinator import NartisConfigEntry, NartisCoordinator
from .entity import NartisEntity

PAIRED = BinarySensorEntityDescription(
    key="paired",
    translation_key="paired",
    device_class=BinarySensorDeviceClass.CONNECTIVITY,
    entity_category=EntityCategory.DIAGNOSTIC,
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: NartisConfigEntry,
    async_add_entities: AddConfigEntryEntitiesCallback,
) -> None:
    async_add_entities([NartisPairedSensor(entry.runtime_data)])


class NartisPairedSensor(NartisEntity, BinarySensorEntity):
    """Whether the device is paired with the meter."""

    entity_description = PAIRED

    def __init__(self, coordinator: NartisCoordinator) -> None:
        super().__init__(coordinator, PAIRED.key)

    @property
    def is_on(self) -> bool:
        return self.coordinator.data.paired
