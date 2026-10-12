# Third-party notices

## firmware/components/nartis_rf/src

Nartis RF433 protocol stack (pairing, framing, CRC, AES-128-GCM, DLMS/COSEM).
Taken from FerrumLogic/esphome-nartis-rf-meter-cc1101
(https://github.com/FerrumLogic/esphome-nartis-rf-meter-cc1101, see UPSTREAM.txt),
a CC1101 port of latonita/esphome-nartis-rf-meter
(https://github.com/latonita/esphome-nartis-rf-meter), licensed under MIT:

```
MIT License

Copyright (c) 2026 Anton Viktorov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## firmware/components/nartis_rf/src/cc1101_vendored.c, cc1101_vendored.h, ccpacket_vendored.h

CC1101 driver based on the panStamp CC1101 library.
Copyright (c) 2011 panStamp, Copyright (c) 2016 Tyler Sommer, author Daniel Berenguer.
Licensed under the GNU Lesser General Public License v3.0 or later
(https://www.gnu.org/licenses/lgpl-3.0.html). The original license headers are kept
in the source files; modifications are marked "Vendoring patch".

## ESP-IDF

Espressif IoT Development Framework, Apache License 2.0
(https://github.com/espressif/esp-idf).
