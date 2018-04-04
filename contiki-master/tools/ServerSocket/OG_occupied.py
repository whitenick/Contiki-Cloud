from urllib2 import urlopen
import urllib2
import serial
import requests

url = 'http://kavzoh.pythonanywhere.com/senior_des/database_page?'
passkey = 'b9c4t5tac1+'
data = 'Tim+321+false'

ser = serial.Serial(
	port='/dev/ttyACM0',\
	baudrate=115200,\
	parity=serial.PARITY_NONE,\
	stopbits=serial.STOPBITS_ONE,\
	bytesize=serial.EIGHTBITS,\
	timeout=0)
print("connected to: " + ser.portstr)
ser.write("help\n")
temp = ""
response = ""
while True:
	line = ser.readline()
	if line:
		response = urlopen(url+passkey+data)
		req = urllib2.Request(url=url+passkey, data=data)
		second_response = urlopen(req)
		print(second_response.read())
		print(r.content) 
		print(line)
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
