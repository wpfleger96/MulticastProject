CC = g++
DEBUG = -g
CFLAGS = -Wall -std=gnu++11 $(DEBUG)
LFLAGS = -Wall $(DEBUG)

all: clean participant coordinator

participant: participant.cpp
	$(CC) $(CFLAGS) -o participant participant.cpp -lpthread

coordinator: coordinator.cpp
	$(CC) $(CFLAGS) -o coordinator coordinator.cpp -lpthread

clean:
	rm -rf participant
	rm -rf coordinator