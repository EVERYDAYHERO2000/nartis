"""Exercise the actual HA phase sensor without installing Home Assistant."""

import ast
import math
from pathlib import Path
from types import SimpleNamespace
import unittest


class Entity:
    def __init__(self, coordinator, key):
        self.coordinator = coordinator
        self.key = key


source = Path(__file__).resolve().parents[1] / "custom_components/nartis/sensor.py"
tree = ast.parse(source.read_text())
sensor_class = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "NartisPhasePowerSensor")
namespace = {
    "NartisEntity": Entity, "SensorEntity": type("SensorEntity", (), {}),
    "NartisCoordinator": SimpleNamespace, "SensorEntityDescription": SimpleNamespace,
    "isfinite": math.isfinite,
}
exec(compile(ast.Module(body=[sensor_class], type_ignores=[]), str(source), "exec"), namespace)
PhaseSensor = namespace["NartisPhasePowerSensor"]


def reading(value, stale=False):
    return SimpleNamespace(value=value, stale=stale)


class PhasePowerTest(unittest.TestCase):
    def setUp(self):
        self.values = {
            "1.0.32.7.0.255": reading(230), "1.0.31.7.0.255": reading(2),
            "1.0.52.7.0.255": reading(220), "1.0.51.7.0.255": reading(3),
            "1.0.72.7.0.255": reading(240), "1.0.71.7.0.255": reading(0),
        }
        self.coordinator = SimpleNamespace(data=SimpleNamespace(values=self.values))

    def sensor(self, phase=1):
        return PhaseSensor(self.coordinator, phase, SimpleNamespace(key=f"apparent_power_l{phase}"))

    def test_phases_and_zero_load(self):
        self.assertEqual([self.sensor(p).native_value for p in (1, 2, 3)], [460, 660, 0])
        sensor = self.sensor()
        self.values["1.0.31.7.0.255"].value = 4
        self.assertEqual(sensor.native_value, 920)

    def test_missing_invalid_or_stale_source(self):
        sensor = self.sensor()
        for value in (None, float("nan"), float("inf")):
            self.values["1.0.31.7.0.255"] = reading(value)
            self.assertIsNone(sensor.native_value)
        self.values["1.0.31.7.0.255"] = reading(2, stale=True)
        self.assertIsNone(sensor.native_value)
        del self.values["1.0.31.7.0.255"]
        self.assertIsNone(sensor.native_value)


if __name__ == "__main__":
    unittest.main()
