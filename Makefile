CFLAGS?=-Wall -g

all:
	$(CC) $(CFLAGS) -I/usr/X11R6/include -L/usr/X11R6/lib -lc -lm -lX11 -o musca musca.c
	$(CC) $(CFLAGS) -I/usr/X11R6/include -L/usr/X11R6/lib -lc -lm -lX11 -o apis apis.c
	$(CC) $(CFLAGS) -I/usr/X11R6/include -L/usr/X11R6/lib -lc -lm -lX11 -o xlisten xlisten.c

clean:
	rm -f musca apis
