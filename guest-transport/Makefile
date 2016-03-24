CFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers

all: drs-transport drs-transport.exe

lin_drs-transport.o: transport.c transport.h
	gcc -c $(CFLAGS) -o lin_drs-transport.o transport.c

lin_transport.o: lin_transport.c transport.h
	gcc -c $(CFLAGS) -o lin_transport.o lin_transport.c

drs-transport: lin_drs-transport.o lin_transport.o
	gcc -o drs-transport lin_drs-transport.o lin_transport.o

win_transport.o: win_transport.c transport.h
	i686-w64-mingw32-gcc -c $(CFLAGS) -o win_transport.o win_transport.c

win_drs-transport.o: transport.c transport.h
	i686-w64-mingw32-gcc -c $(CFLAGS) -o win_drs-transport.o transport.c

drs-transport.exe: win_drs-transport.o win_transport.o
	i686-w64-mingw32-gcc -o drs-transport.exe win_drs-transport.o win_transport.o

clean:
	rm -f *.o *.exe drs-transport

.PHONY: all clean
