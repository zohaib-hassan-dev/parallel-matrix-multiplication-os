CC=g++
CFLAGS=-std=c++11 -O2 -Wall
TARGET=master
SERIAL=serial

all: $(TARGET) $(SERIAL)

$(TARGET): src/master.cpp
	$(CC) $(CFLAGS) src/master.cpp -o $(TARGET)

$(SERIAL): src/serial.cpp
	$(CC) $(CFLAGS) src/serial.cpp -o $(SERIAL)

clean:
	rm -f $(TARGET) $(SERIAL) *.o gui/results.json gui/progress.json
