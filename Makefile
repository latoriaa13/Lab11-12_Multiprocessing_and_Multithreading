CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -g -MMD -MP
LDLIBS := -ljpeg

PROGRAMS := mandel mandelmovie
OBJECTS := mandel.o jpegrw.o mandelmovie.o

.PHONY: all clean

all: $(PROGRAMS)

mandel: mandel.o jpegrw.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

mandelmovie: mandelmovie.o
	$(CC) $(LDFLAGS) $^ -lm -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -f $(OBJECTS) $(OBJECTS:.o=.d) $(PROGRAMS)
