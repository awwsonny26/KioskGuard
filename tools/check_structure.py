#!/usr/bin/env python3
"""Check portable layers and .ioc / Wokwi pin agreement."""
import json
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
for layer in ("App", "Devices"):
    for source in (root / "firmware" / layer).glob("*.[ch]"):
        text = source.read_text()
        for header in re.findall(r'#include\s*["<]([^">]+)', text):
            assert not any(word in header.lower() for word in
                           ("hal", "cmsis", "platform", "core/", "stm32")), (source, header)
        assert not re.search(r"\b(?:HAL_|LL_|GPIO[A-F]|I2C1|USART2)\w*", text), source
diagram = json.loads((root / "diagram.json").read_text())
connections = {frozenset(c[:2]) for c in diagram["connections"]}
for a, b in [("imu:SCL", "nucleo:PB8"), ("imu:SDA", "nucleo:PB9"),
             ("pir:OUT", "nucleo:PA1"), ("light:AO", "nucleo:PA0")]:
    assert frozenset((a,b)) in connections, (a,b)
ioc = (root / "firmware/firmware.ioc").read_text()
for setting in ("PA0.Signal=ADC1_IN0", "PA1.Signal=GPIO_Input",
                "PB8.Signal=I2C1_SCL", "PB9.Signal=I2C1_SDA",
                "PA2.Signal=USART2_TX", "PA3.Signal=USART2_RX"):
    assert setting in ioc, setting
print("PASS: portable dependencies and sensor wiring")
