# Power Sensor

This sketch reads the value from the analog input representing current and sends the calculated wattage to the
established REST endpoint.

# Hardware 

The sketch runs on an ESP8266 which receives current data from an ACS712 device.  Both devices are powered by a buck transformer which converts 110VAC to 5V DC.  The ACS712 sensor range is -30 to +30 Amps, and will output 0V when current is -30A, and 5V when current is +30A.  Resistors are used to reduce the ACS712 output voltage to 0-3.3V, which is the voltage range expected by the ESP8266 A0 input pin.

## Assumptions:

Household is 110Vrms

Load is Resistance only - current is in phase with voltage

Sensor range is +/- 30Amp

## Approach for Power Calculation:

400 samples are taken from the analog input to establish the max and min current.  This takes about 40ms, so more than one cycle of the 60Hz circuit is sampled.

Power is:  Irms * Vrms

Irms = 1/2 (peak to peak amplitude) / (square root of 2)

When no load is present, the ACS712 current sensor will output Vcc/2 which should be 5V/2 = 2.5V.  It should output Vcc at +30amps, and 0V at -30Amps.  The ESP8266 anolog input range, however, is 0-3.3V.  Resistors are used to reduce the output voltage from the ACS712 device to the ESP8266 accordingly. The analog input converts the 0-3.3V input into an integer of the range 0-1023, where 1023 represents the max voltage.

### Calibration (TODO)

The buck transformer (converts 110V AC to 5V DC to power both the ACS712 and ESP8266 MCU) input voltage, actual resistor variations, and variations in the ACS712 device usually result in the no-load input not falling exactly at 1.65V (512 from the analog input, whose range is 0-1023).  If, for example, the no load input is 540 from the analog input, this value should be assumed to be the new midpoint of the input range, and power calculations should be adjusted accordingly.  In this case, +30amps would be assumed to be read as 1080 (with the understanding that this is not possible; if this is the case then our device would not be able to distinguish 29.5A from 30A because both would read 1023) and 0 would always represent -30Amps.

## Resources:
https://www.instructables.com/Arduino-Energy-Meter-V20/

https://create.arduino.cc/projecthub/SurtrTech/measure-any-ac-current-with-acs712-70aa85

## TODO

### Security

Prevent any party from sending data. 

The electric company or organization providing the power devices would presumbably maintain a record of the MAC addresses of each device delivered to each customer.  The Device setup step should also include transmittal of registration data including customer-entered billing account, appliance name (e.g., Refrigerator) and the device MAC address to a separate endpoint dedicated to registration.  The registration endpoint should validate the customer MAC and billing account match, and respond with a newly generated unique ID assigned to the device.  The device should save this unique ID for later metric reporting.  

The Data metric endpoint should receive the assigned device ID, wattage data, and a SHA256 hash of the deviceID+MAC+timestamp (to the second) from the device.  The device is currently sending MAC and watts to the metric endpoint.  The metric endpoint should validate the device by performing the same hash for current time and ensuring the received hash matches.  If it doesn't, the hash could be generated using timestamps 1 and maybe 2 seconds in the past and checking that they match again. If none of the hashes match, the endpoint should ignore the metric data.

### Calibration

The calibration described above is not yet implemented.  The analog input range representing -30 to +30 amps is currently assumed to be 0-1023.  The device setup form should have an option to calibrate, at which time the customer should disconnect any load.

### No Connectivity to Metrics endpoint

The ESP8266 should be able to save almost 2MB of data points if it for some reason lost connectivity to the metrics endpoint.  The metrics endpoint should support receiving a historical list of data points that the device had saved during any connectivity downtime.
