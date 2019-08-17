.SUFFIXES: .c $(SUFFIXES)

TARGET		= mx44

PREFIX		= /usr/local/

DATADIR		= $(PREFIX)share/Mx44/
BINDIR		= $(PREFIX)bin/

DIR		= Mx44.2

CC		= gcc

GTK_CFLAGS = `pkg-config --cflags gtk+-2.0` #-DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED
GTK_LIBS   = `pkg-config --libs gtk+-2.0`

ALSA_CFLAGS = `pkg-config --cflags alsa`
ALSA_LIBS   = `pkg-config --libs alsa`

JACK_CFLAGS = `pkg-config --cflags jack`
JACK_LIBS   = `pkg-config --libs jack`


LIBS		= $(GTK_LIBS) $(ALSA_LIBS) $(JACK_LIBS) -lm 

FLAGS		= -O3 -Wall -pipe -mmmx -fomit-frame-pointer -D_REENTRANT \
		   -DDATADIR=\"$(DATADIR)\" \
		   $(GTK_CFLAGS) $(ALSA_CFLAGS) $(JACK_CFLAGS) 

SRCS            =  interface2.c mx44.c stateio.c cmdline_opts.c main.c
INCLUDE         =  mx44.h

OBJS		= $(SRCS:.c=.o)




$(TARGET):	$(OBJS)
	$(CC) -D_REENTRANT -o $@ $(OBJS) $(LIBS)

.c.o: 
	$(CC) -c $(FLAGS) $<

install: $(TARGET)
	install $(TARGET) $(BINDIR)
	install -d $(DATADIR)
	install -t $(DATADIR) ../data/mx44patch
	install -t $(DATADIR) ../data/gtk-2.0/gtkrc

clean:
	rm -f $(TARGET) $(OBJS) core core.* *.bak *~

depend:
	makedepend  -Y  $(SRCS)  &>/dev/null

edit:
	exec emacs -font 9x15 $(INCLUDE) $(SRCS) Makefile&

etag:
	rm TAGS;etags --members  $(SRCS) $(INCLUDE)    


tar:
	cd ../..; 			\
	rm -f $(DIR).tar.gz ; 		\
	tar -cf $(DIR).tar 		\
		$(DIR)/src/*.c 		\
		$(DIR)/src/*.h 		\
		$(DIR)/README 		\
		$(DIR)/src/Makefile 	\
		$(DIR)/COPYING 		\
		$(DIR)/data/mx44patch 	\
		$(DIR)/data/gtk-2.0/gtkrc ;\
	gzip $(DIR).tar

# DO NOT DELETE

interface2.o: mx44.h mmx.h
mx44.o: mx44.h mmx.h interface2.h cmdline_opts.h
stateio.o: stateio.h mx44.h mmx.h
cmdline_opts.o: cmdline_opts.h
main.o: mx44.h mmx.h interface2.h stateio.h cmdline_opts.h
