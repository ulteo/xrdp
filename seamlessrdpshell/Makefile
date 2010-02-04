OBJS = main.o xutils.o user_channel.o


DESTDIR = .

CFLAGS = -Wall -O2 -D_REENTRANT
LDFLAGS = -lX11 -lpthread
CC = gcc



all : XHook


XHook : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o XHook $(OBJS) $(LIBS)


clean : 
	rm -fr *.o XHook
