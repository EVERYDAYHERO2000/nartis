"""Config flow for the Nartis RF Meter integration."""

from __future__ import annotations

from typing import Any

import voluptuous as vol

from homeassistant.config_entries import ConfigEntry, ConfigFlow, ConfigFlowResult, OptionsFlow
from homeassistant.const import CONF_HOST
from homeassistant.core import callback
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .api import NartisApi, NartisApiError
from .const import (
    CONF_SCAN_INTERVAL,
    DEFAULT_SCAN_INTERVAL,
    DOMAIN,
    MAX_SCAN_INTERVAL,
    MIN_SCAN_INTERVAL,
)


class NartisConfigFlow(ConfigFlow, domain=DOMAIN):
    """Add a Nartis device by its IP address or host name."""

    VERSION = 1

    async def async_step_user(self, user_input: dict[str, Any] | None = None) -> ConfigFlowResult:
        errors: dict[str, str] = {}
        if user_input is not None:
            host = user_input[CONF_HOST].strip()
            api = NartisApi(async_get_clientsession(self.hass), host)
            try:
                readings = await api.async_get_readings()
            except NartisApiError:
                errors["base"] = "cannot_connect"
            else:
                if not isinstance(readings.get("values"), list):
                    errors["base"] = "not_nartis"
                else:
                    serial = str(readings.get("serial") or "")
                    await self.async_set_unique_id(serial or host)
                    self._abort_if_unique_id_configured(updates={CONF_HOST: host})
                    title = f"Нартис {serial}" if serial else f"Нартис ({host})"
                    return self.async_create_entry(title=title, data={CONF_HOST: host})

        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema({vol.Required(CONF_HOST): str}),
            errors=errors,
        )

    @staticmethod
    @callback
    def async_get_options_flow(config_entry: ConfigEntry) -> OptionsFlow:
        return NartisOptionsFlow()


class NartisOptionsFlow(OptionsFlow):
    """Change the polling interval."""

    async def async_step_init(self, user_input: dict[str, Any] | None = None) -> ConfigFlowResult:
        if user_input is not None:
            return self.async_create_entry(data=user_input)

        current = self.config_entry.options.get(CONF_SCAN_INTERVAL, DEFAULT_SCAN_INTERVAL)
        return self.async_show_form(
            step_id="init",
            data_schema=vol.Schema(
                {
                    vol.Required(CONF_SCAN_INTERVAL, default=current): vol.All(
                        vol.Coerce(int), vol.Range(min=MIN_SCAN_INTERVAL, max=MAX_SCAN_INTERVAL)
                    )
                }
            ),
        )
