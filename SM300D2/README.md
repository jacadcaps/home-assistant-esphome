# SM300D2 for Home Assistant / ESPHome

esphome has SM300D2 support built-in, but I've noticed there's several issues with the driver - 
there does not seem to be any way of recovering on errors and it looks like the device likes to
randomly spam UART. Once that happens, the included driver will just stop updating Hass states
until the ESP board is rebooted.

This driver does several things to improve the stability and data sent to Hass:

  - Attempts to establish start of the SM300D2 frame, dropping useless data
  - Rejects frames with invalid CRC
  - Rejects frames with read-outs that are out of bounds (as defined in SM300D2 specs)
  - Uses moving averages to smooth out the data sent to Hass (can be configured in the source)

esphome configuration:

```yaml
esphome:
  ...
  includes:
    - sm300d2.h

...

# Setup an UART to communicate (only RX is actually used)
uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

# Define the sensor
sensor:
- platform: custom
  lambda: |-
    auto s = new SM300D2(id(uart_bus));
    App.register_component(s);
    return { s->s(0), s->s(1), s->s(2), s->s(3), s->s(4), s->s(5), s->s(6) };

  sensors:
  - name: "PM2.5"
    unit_of_measurement: μg/m3
    icon: "mdi:factory"
  - name: "PM10.0"
    unit_of_measurement: μg/m3
    icon: "mdi:factory"
  - name: "CO2"
    unit_of_measurement: ppm
    icon: "mdi:molecule-co2"
  - name: "CH2O"
    unit_of_measurement: μg/m3
    icon: "mdi:factory"
  - name: "TVOC"
    unit_of_measurement: μg/m3
    icon: "mdi:factory"
  - name: "Temperature"
    unit_of_measurement: °C
    accuracy_decimals: 1
    icon: "mdi:temperature-celsius"
  - name: "Humidity"
    unit_of_measurement: "%"
    accuracy_decimals: 1    
    icon: "mdi:water-percent"
```
