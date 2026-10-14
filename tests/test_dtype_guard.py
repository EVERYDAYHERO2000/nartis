"""Host regression for the firmware's batch retry/type guard (no ESP-IDF)."""

import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / "firmware/components/nartis_rf/src"


def block(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class BatchTypeGuardTest(unittest.TestCase):
    def test_fixed_types_survive_skips_and_reject_voltage_reply(self):
        header = (SRC / "nartis_rf_meter.h").read_text()
        source = (SRC / "nartis_rf_meter.cpp").read_text()
        setter = block(header, "void set_sensor_expected_dtype(")
        skip = block(source, "void NartisRfMeterComponent::skip_current_batch_()")
        guard = block(source, "for (uint8_t i = 0; i < got_count && (batch_start_idx_ + i) < sensors_.size(); i++)")
        # Compile the production setter, skip routine and response guard with
        # small host substitutes for the radio/ESP-IDF environment.
        program = r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <vector>
#define ESP_LOGW(...) ((void)0)
struct Value { uint8_t dtype; bool valid; bool has_value() const { return valid; } };
struct SensorEntry { uint8_t expected_dtype{0}; bool expected_dtype_fixed{false}; };
class NartisRfMeterComponent {
public:
  std::vector<SensorEntry> sensors_{11};
  bool session_primed_{true};
  uint8_t batch_start_idx_{5}, batch_count_{5};
  SETTER
  void skip_current_batch_();
  bool accepts(const std::vector<Value>& values) {
    uint8_t got_count = values.size();
    GUARD
    return true;
  }
};
SKIP
int main() {
  NartisRfMeterComponent c;
  uint8_t types[] = {5, 18, 18, 18, 6, 6, 6, 6, 6, 6, 18};
  for (size_t i=0; i<11; ++i) c.set_sensor_expected_dtype(i, types[i]);
  std::vector<Value> voltage = {{5,true},{18,true},{18,true},{18,true},{6,true}};
  std::vector<Value> energy(5, {6,true});
  for (int retry=0; retry<4; ++retry) {
    c.batch_start_idx_=5;
    assert(!c.accepts(voltage));
    c.skip_current_batch_();
    assert(c.batch_start_idx_==10);
    c.batch_start_idx_=5;
    assert(!c.accepts(voltage));
    assert(c.accepts(energy));
    for (size_t i=5; i<10; ++i) assert(c.sensors_[i].expected_dtype==6);
  }
  c.sensors_[5]={18,false}; // Inferred types may still be relearned.
  c.skip_current_batch_();
  assert(c.sensors_[5].expected_dtype==0);
  assert(c.sensors_[7].expected_dtype==6);
  c.session_primed_=false;
  c.batch_start_idx_=0;
  c.skip_current_batch_();
  assert(c.batch_start_idx_==0);
}
'''.replace("SETTER", setter).replace("GUARD", guard).replace("SKIP", skip)
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "guard.cpp"
            executable = pathlib.Path(directory) / "guard"
            cpp.write_text(program)
            subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
