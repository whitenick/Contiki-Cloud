from urllib2 import urlopen
import urllib2
import serial
import requests


url = 'http://karan41.pythonanywhere.com/senior_des/database_page?'
passkey = 'b9c4t5tac1+'
data = 'Nick+2202+true'

ser = serial.Serial(
	port='/dev/ttyACM0',\
	baudrate=115200,\
	parity=serial.PARITY_NONE,\
	stopbits=serial.STOPBITS_ONE,\
	bytesize=serial.EIGHTBITS,\
	timeout=None)
print("connected to: " + ser.portstr)
ser.write("help\n")
message_count = 0
message_id = []
while (True):
	ser.flush()
	line = ser.readline()
	lines = line.split("+")
	print(lines)
	if len(lines) == 3:
		line = lines[0]+"+"+lines[2]
	if line:
#line = lines[0]+"+"+lines[1]+"+"+lines[3]
		message_count += 1
		print(message_count)
		response = urlopen(url+passkey+line)
		print(line),
		print(response.read())		
ser.close()

#Authentication (Unnecessary now - i unencrypted the server)
#
# username = 'kavzoh'
# password = 'password'
# top_url = 'http://kavzoh.pythonanywhere.com/senior_des/home_page'
# manager = urllib.request.HTTPPasswordMgrWithDefaultRealm()
# manager.add_password(None, top_url, username, password)
# auth_handler = urllib.request.HTTPBasicAuthHangler(manager)
# opener = urllib.request.build_opener(auth_handler)
# urlli.request.install_opener(opener)
# response = opener.open(url+passkey+data)
# print (response.read())
