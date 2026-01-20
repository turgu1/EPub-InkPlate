#!/bin/sh
#
# Script to upload a new application binary to an ESP32 device.
#
# Three files must be uploaded:
#
#  - Bootstrap program
#  - Flash memory partitioning description
#  - Application program
#
# Do not put any spaces in the following definitions.
#
# You may have to modify the device name to which the 
# InkPlate device is connected. Look below and adjust 
# the "device=" entry.
#
# Guy Turcotte, January 2021.
#

device=/dev/ttyUSB0
speed=230400

bootstrap=bootloader.bin
partitions=partitions.bin
program=firmware.bin

bootstrap_location=0x1000
partitions_location=0x8000
program_location=0x10000

esptool.py --chip esp32 --port ${device} --baud ${speed} --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect ${bootstrap_location} ${bootstrap} ${partitions_location} ${partitions} ${program_location} ${program}
