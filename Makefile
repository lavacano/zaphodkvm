CFLAGS = -g

all: zaphodkvm

zaphodkvm: zaphodkvm.cpp Util.o suinput.o
	g++ -o zaphodkvm zaphodkvm.cpp Util.o suinput.o -lpthread -ludev $(CFLAGS)

Util.o: Util.cpp Util.h
	g++ -c Util.cpp $(CFLAGS)

suinput.o: suinput.c suinput.h
	g++ -c suinput.c $(CFLAGS)

clean:
	rm -fr *.o zaphodkvm
