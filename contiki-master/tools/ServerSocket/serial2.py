#!/usr/bin/python
import serial
ser = serial.Serial(
   port='/dev/ttyACM0',\
   baudrate=115200,\
   parity=serial.PARITY_NONE,\
   stopbits=serial.STOPBITS_ONE,\
   bytesize=serial.EIGHTBITS,\
   timeout=0)
print("connected to: " + ser.portstr)
ser.write("help\n");
while True:
	line = ser.readline();
	if line:
		print(line),
ser.close()
