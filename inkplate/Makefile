compile: inkplate.ino cat.h
	arduino-cli compile --build-path ./build --build-cache-path ./cache --fqbn Inkplate_Boards:esp32:Inkplate10 inkplate.ino -v

upload: inkplate.ino cat.h
	arduino-cli compile --build-path ./build --build-cache-path ./cache --fqbn Inkplate_Boards:esp32:Inkplate10 -u -p /dev/ttyUSB0 inkplate.ino -v
