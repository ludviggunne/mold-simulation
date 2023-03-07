CC = gcc
CFLAGS = -Wall

SOURCES = main.c glad/src/glad.c

OBJECTS = main.o glad.o

INCLUDE_DIRS = glad/include/

LIBS = glfw m

default: main.c
	$(CC) $(CFLAGS) $(addprefix -I, $(INCLUDE_DIRS)) -c $(SOURCES)
	$(CC) $(OBJECTS) $(addprefix -l, $(LIBS)) -o main 

clean:
	rm -r -f $(OBJECTS) main