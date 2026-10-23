# Wiring and testing notes

## Block diagram

Solar panel → compatible solar charge controller → 12 V battery → fused DC load path → relay/switch → LED lamp

Voltage divider, current sensor, and temperature sensor → ESP32 → Wi-Fi / serial diagnostics → dashboard (future integration)

## Starter pin mapping
- Battery voltage divider midpoint → ESP32 GPIO 34 (ADC input only)
- DS18B20 data → GPIO 4, with a 4.7 kΩ pull-up to 3.3 V
- Optional ACS712 analog output → GPIO 35 (ADC input only); confirm output never exceeds ESP32's 3.3 V ADC range
- Relay module input → GPIO 26
- All low-voltage sensor grounds and ESP32 ground must share a reference where appropriate. Never connect battery voltage directly to an ESP32 pin.

## Voltage divider
For the suggested 100 kΩ / 27 kΩ divider, the divider ratio is (100 + 27) / 27 ≈ 4.704. A 14.5 V input is about 3.08 V at the ADC node. Use appropriately rated resistors, check the actual ratio with a meter, and verify the maximum voltage cannot exceed the ESP32 ADC input limit. ADC calibration varies by ESP32 board and must be checked in firmware.

## Safety checklist
- Put a fuse close to the battery positive terminal, rated for the wire and expected load.
- Use a charge controller designed for the battery chemistry and panel.
- Use a DC-rated relay/MOSFET switch rated for the load's voltage and current; AC-only ratings are not sufficient.
- Keep battery protection independent of the dashboard/network. Prefer a proper battery-management/protection device where applicable.
- Confirm relay logic and normally-open contact behavior with no battery/load connected.
- Test cut-off and reconnect logic with a current-limited bench supply or simulated readings first.
- Stop and get qualified supervision if the battery heats, swells, leaks, smells unusual, or wiring becomes hot.
- Do not short the battery or test faults by intentionally over-discharging a real battery.

## Limitations
This is a hackathon starter, not a certified battery management system. It does not estimate battery state-of-health and does not replace the charge controller, BMS, fuse, or professional electrical review.
