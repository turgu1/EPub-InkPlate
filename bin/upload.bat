:: Script to upload a new application binary to an ESP32 device.
::
:: Three files must be uploaded:
::
::  - Bootstrap program
::  - Flash memory partitioning description
::  - Application program
::
:: Do not put any spaces in the following definitions.
::
:: You may have to modify the device port number to which the 
:: InkPlate device is connected. Look below and adjust 
:: the "set device=" entry.
::
:: Guy Turcotte, January 2021.

@echo off
set device=COM3
set speed=230400

set bootstrap=bootloader.bin
set partitions=partitions.bin
set program=firmware.bin

set bootstrap_location=0x1000
set partitions_location=0x8000
set program_location=0x10000

python -m esptool --chip esp32 --port %device% --baud %speed% --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect %bootstrap_location% %bootstrap% %partitions_location% %partitions% %program_location% %program%
