#!/usr/bin/python
import socket
import urllib
import requests
import serial


#sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#ip = socket.gethostbyname('localhost')

#port = 60001
#sock.bind(('', port))
#print 'socket bound'

#sock.listen(1)
#c, addr = sock.accept()
ser = serial.Serial(
    port='/dev/ttyACM0',\
    baudrate=115200,\
    parity=serial.PARITY_NONE,\
    stopbits=serial.STOPBITS_ONE,\
    bytesize=serial.EIGHTBITS,\
    timeout=0)
print("connected to: " + ser.portstr)
ser.write("help\n")

while True:
    #print 'Got connection from', addr
    #readString = c.recv(1024)
    #print readString
    line = ser.readline();
    if line:
        print(line),

    base_url="http://kavzoh.pythonanywhere.com/senior_des/database_page?b9c4t5tac1"

    payload = line
    response = requests.post(base_url, data=payload)

   #print(response.text)
ser.close()
