CC = gcc
CFLAGS = -Wall -O2
TARGET = ccsh
SRC = ccsh.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
