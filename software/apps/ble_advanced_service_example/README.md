BLE Service Example App
=======================

Example of using a BLE service and characteristic to allow an LED to be
controlled over a connection. Outputs initialization steps and events through
RTT. By default the BLE address will be `c0:98:e5:4e:aa:bb` and the device name
will be `CS397/497`.

Note that the device's BLE address must be unique among BLE devices in the
area. If you are programming multiple boards, change the `.device_id` portion
of the name.
