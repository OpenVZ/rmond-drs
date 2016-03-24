CFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers

drs-transport: transport.c transport.h
	gcc $(CFLAGS) -o drs-transport transport.c

all: drs-transport

clean:
	rm -f drs-transport.exe drs-transport

.PHONY: all clean
