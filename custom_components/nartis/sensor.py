"""Sensors for the Nartis RF Meter integration."""

from __future__ import annotations

from datetime import datetime
from math import isfinite

from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorEntityDescription,
    SensorStateClass,
)
from homeassistant.const import (
    SIGNAL_STRENGTH_DECIBELS_MILLIWATT,
    EntityCategory,
    UnitOfElectricCurrent,
    UnitOfElectricPotential,
    UnitOfEnergy,
    UnitOfFrequency,
    UnitOfPower,
    UnitOfApparentPower,
)
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddConfigEntryEntitiesCallback

from .coordinator import NartisConfigEntry, NartisCoordinator
from .entity import NartisEntity


def _power(key: str) -> SensorEntityDescription:
    return SensorEntityDescription(
        key=key, translation_key=key, device_class=SensorDeviceClass.POWER,
        state_class=SensorStateClass.MEASUREMENT, native_unit_of_measurement=UnitOfPower.WATT,
        suggested_display_precision=0,
    )


def _voltage(key: str) -> SensorEntityDescription:
    return SensorEntityDescription(
        key=key, translation_key=key, device_class=SensorDeviceClass.VOLTAGE,
        state_class=SensorStateClass.MEASUREMENT, native_unit_of_measurement=UnitOfElectricPotential.VOLT,
        suggested_display_precision=1,
    )


def _current(key: str) -> SensorEntityDescription:
    return SensorEntityDescription(
        key=key, translation_key=key, device_class=SensorDeviceClass.CURRENT,
        state_class=SensorStateClass.MEASUREMENT, native_unit_of_measurement=UnitOfElectricCurrent.AMPERE,
        suggested_display_precision=2,
    )


def _energy(key: str) -> SensorEntityDescription:
    return SensorEntityDescription(
        key=key, translation_key=key, device_class=SensorDeviceClass.ENERGY,
        state_class=SensorStateClass.TOTAL_INCREASING, native_unit_of_measurement=UnitOfEnergy.KILO_WATT_HOUR,
        suggested_display_precision=1,
    )


# Values published by the firmware, keyed by OBIS code. The firmware already
# applies the scaling, so values arrive in the units below.
OBIS_SENSORS: dict[str, SensorEntityDescription] = {
    "1.0.1.7.0.255": _power("power"),
    "1.0.32.7.0.255": _voltage("voltage_l1"),
    "1.0.52.7.0.255": _voltage("voltage_l2"),
    "1.0.72.7.0.255": _voltage("voltage_l3"),
    "1.0.31.7.0.255": _current("current_l1"),
    "1.0.51.7.0.255": _current("current_l2"),
    "1.0.71.7.0.255": _current("current_l3"),
    "1.0.1.8.0.255": _energy("energy_total"),
    "1.0.1.8.1.255": _energy("energy_t1"),
    "1.0.1.8.2.255": _energy("energy_t2"),
    "1.0.14.7.0.255": SensorEntityDescription(
        key="frequency", translation_key="frequency", device_class=SensorDeviceClass.FREQUENCY,
        state_class=SensorStateClass.MEASUREMENT, native_unit_of_measurement=UnitOfFrequency.HERTZ,
        suggested_display_precision=2,
    ),
}

PHASE_POWER_SENSORS = tuple(
    SensorEntityDescription(
        key=f"apparent_power_l{phase}", translation_key=f"apparent_power_l{phase}",
        device_class=SensorDeviceClass.APPARENT_POWER,
        state_class=SensorStateClass.MEASUREMENT,
        native_unit_of_measurement=UnitOfApparentPower.VOLT_AMPERE,
        suggested_display_precision=0,
    )
    for phase in (1, 2, 3)
)

LAST_READING = SensorEntityDescription(
    key="last_reading", translation_key="last_reading",
    device_class=SensorDeviceClass.TIMESTAMP, entity_category=EntityCategory.DIAGNOSTIC,
)

WIFI_RSSI = SensorEntityDescription(
    key="wifi_rssi", translation_key="wifi_rssi",
    device_class=SensorDeviceClass.SIGNAL_STRENGTH, state_class=SensorStateClass.MEASUREMENT,
    native_unit_of_measurement=SIGNAL_STRENGTH_DECIBELS_MILLIWATT,
    entity_category=EntityCategory.DIAGNOSTIC, entity_registry_enabled_default=False,
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: NartisConfigEntry,
    async_add_entities: AddConfigEntryEntitiesCallback,
) -> None:
    """Create one sensor per OBIS value reported by the device, plus diagnostics."""
    coordinator = entry.runtime_data
    entities: list[SensorEntity] = []
    for obis, value in coordinator.data.values.items():
        description = OBIS_SENSORS.get(obis)
        if description is not None:
            entities.append(NartisObisSensor(coordinator, obis, description))
        else:
            entities.append(NartisGenericSensor(coordinator, obis, value.label, value.unit))
    entities.append(NartisLastReadingSensor(coordinator))
    entities.append(NartisRssiSensor(coordinator))
    for phase, description in enumerate(PHASE_POWER_SENSORS, start=1):
        entities.append(NartisPhasePowerSensor(coordinator, phase, description))
    async_add_entities(entities)


class NartisPhasePowerSensor(NartisEntity, SensorEntity):
    """Calculated apparent power U × I for one phase, in VA."""

    def __init__(self, coordinator: NartisCoordinator, phase: int, description: SensorEntityDescription) -> None:
        super().__init__(coordinator, description.key)
        self.entity_description = description
        voltage_code = 32 + (phase - 1) * 20
        self._voltage_obis = f"1.0.{voltage_code}.7.0.255"
        self._current_obis = f"1.0.{voltage_code - 1}.7.0.255"

    @property
    def native_value(self) -> float | None:
        voltage = self.coordinator.data.values.get(self._voltage_obis)
        current = self.coordinator.data.values.get(self._current_obis)
        if voltage is None or current is None or voltage.stale or current.stale:
            return None
        if voltage.value is None or current.value is None:
            return None
        if not isfinite(voltage.value) or not isfinite(current.value):
            return None
        power = voltage.value * current.value
        return power if isfinite(power) else None


class NartisObisSensor(NartisEntity, SensorEntity):
    """A known meter value (power, voltage, current, energy, frequency)."""

    def __init__(self, coordinator: NartisCoordinator, obis: str, description: SensorEntityDescription) -> None:
        super().__init__(coordinator, description.key)
        self.entity_description = description
        self._obis = obis

    @property
    def native_value(self) -> float | None:
        value = self.coordinator.data.values.get(self._obis)
        return value.value if value else None


class NartisGenericSensor(NartisEntity, SensorEntity):
    """A value the integration does not know (OBIS list changed in firmware)."""

    _attr_state_class = SensorStateClass.MEASUREMENT

    def __init__(self, coordinator: NartisCoordinator, obis: str, label: str, unit: str) -> None:
        super().__init__(coordinator, "obis_" + obis.replace(".", "_"))
        self._obis = obis
        self._attr_name = label
        self._attr_native_unit_of_measurement = unit or None

    @property
    def native_value(self) -> float | None:
        value = self.coordinator.data.values.get(self._obis)
        return value.value if value else None


class NartisLastReadingSensor(NartisEntity, SensorEntity):
    """When the device last read the meter."""

    entity_description = LAST_READING

    def __init__(self, coordinator: NartisCoordinator) -> None:
        super().__init__(coordinator, LAST_READING.key)

    @property
    def native_value(self) -> datetime | None:
        return self.coordinator.last_reading


class NartisRssiSensor(NartisEntity, SensorEntity):
    """Wi-Fi signal strength of the device."""

    entity_description = WIFI_RSSI

    def __init__(self, coordinator: NartisCoordinator) -> None:
        super().__init__(coordinator, WIFI_RSSI.key)

    @property
    def native_value(self) -> int | None:
        return self.coordinator.data.rssi
