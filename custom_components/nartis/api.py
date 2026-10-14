"""Client for the Nartis RF Meter firmware REST API."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

import aiohttp

from .const import REQUEST_TIMEOUT


class NartisApiError(Exception):
    """The device could not be reached or returned an invalid response."""


@dataclass
class NartisValue:
    """One meter value as reported by /api/readings."""

    obis: str
    label: str
    unit: str
    value: float | None
    decimals: int | None = None
    stale: bool = False


@dataclass
class NartisData:
    """Combined snapshot of /api/readings and /api/status."""

    serial: str
    paired: bool
    age: int | None                      # seconds since the last meter read, None = never
    values: dict[str, NartisValue] = field(default_factory=dict)  # keyed by OBIS
    rssi: int | None = None
    ip: str | None = None
    mode: str | None = None


class NartisApi:
    """Minimal async client for the device's local REST API."""

    def __init__(self, session: aiohttp.ClientSession, host: str) -> None:
        self._session = session
        self._base = host if host.startswith(("http://", "https://")) else f"http://{host}"
        self._base = self._base.rstrip("/")

    @property
    def base_url(self) -> str:
        return self._base

    async def _get(self, path: str) -> dict[str, Any]:
        try:
            async with self._session.get(
                f"{self._base}{path}",
                timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT),
            ) as resp:
                resp.raise_for_status()
                data = await resp.json(content_type=None)
        except (aiohttp.ClientError, TimeoutError, ValueError) as err:
            raise NartisApiError(f"{path}: {err}") from err
        if not isinstance(data, dict):
            raise NartisApiError(f"{path}: unexpected response")
        return data

    async def async_get_readings(self) -> dict[str, Any]:
        return await self._get("/api/readings")

    async def async_get_status(self) -> dict[str, Any]:
        return await self._get("/api/status")

    async def async_get_data(self) -> NartisData:
        readings = await self.async_get_readings()
        try:
            status = await self.async_get_status()
        except NartisApiError:
            status = {}

        values: dict[str, NartisValue] = {}
        for item in readings.get("values") or []:
            obis = item.get("obis")
            if not obis:
                continue
            raw = item.get("value")
            values[obis] = NartisValue(
                obis=obis,
                label=item.get("label") or obis,
                unit=item.get("unit") or "",
                value=float(raw) if isinstance(raw, (int, float)) else None,
                decimals=item.get("decimals"),
                stale=bool(item.get("stale", False)),
            )

        age = readings.get("age")
        rssi = status.get("rssi")
        return NartisData(
            serial=str(readings.get("serial") or ""),
            paired=bool(readings.get("paired")),
            age=int(age) if isinstance(age, (int, float)) else None,
            values=values,
            rssi=int(rssi) if isinstance(rssi, (int, float)) and rssi != 0 else None,
            ip=status.get("ip"),
            mode=status.get("mode"),
        )
