import time
import arduino_bridge

# set pin 11 as 20 ws2812, all with unique colors
LC=18
ROUNDS=10
DELAY=0.02
pin = 11

arduino = arduino_bridge.connection()
arduino.setup_ws2812_common_color_output(pin,LC)

# create a color array, rainbow for demonstration
colorArray = []
MAX=(255//LC)*LC
STEP=MAX//LC
for i in range(0,LC//3):
	colorArray.append(arduino_bridge.Color(MAX-STEP*i, STEP*i, 0))
for i in range(0,6):
	colorArray.append(arduino_bridge.Color(0, MAX-STEP*i, STEP*i))
for i in range(0,6):
	colorArray.append(arduino_bridge.Color(STEP*i, 0, MAX-STEP*i))

# send it
a=0
for ii in range(0,ROUNDS*255):
	print("Round "+str(ii))
	# set the current array
	arduino.ws2812set(pin,colorArray[0])
		
	# rotate all colors by one
	temp = colorArray[0]
	for i in range(0,len(colorArray)-1):
		colorArray[i]=colorArray[i+1]
	colorArray[len(colorArray)-2]=temp
		
	# wait a little
	time.sleep(DELAY)
