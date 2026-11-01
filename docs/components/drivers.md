# Drivers

`components/drivers` currently contains one device: the SHT3x-DIS temperature and humidity sensor (SHT30, SHT31, and SHT35 in single-shot mode).

`drivers::Sht3xSensor::measure()` returns `drivers::EnvironmentSample`:

- `temperature_celsius`
- `relative_humidity_percent`
- `valid`

The driver does not return a `domain::Reading` and it does not know about workers. The bus is `drivers::I2cBus` plus `drivers::Delay`, so host tests can fake the wire. CRC failures and short frames come back invalid. A failed write does not wait for the conversion.

The sensor-node composition, not this component, stamps sample time and origin when it builds a `domain::Reading`.

Synthetic sensors and the simulated output live in `applications/node` (`node_drivers`). They are example code, not this driver.
