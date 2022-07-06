NAME = mpd-status.1s.bin
CFLAGS = -std=c99 -O2 -Wall
LIBS = -lmpdclient
XBARDIR = $(HOME)/Library/Application\ Support/xbar/plugins

$(NAME): main.c
	$(CC) $(CFLAGS) $< $(LIBS) -o $@

install: $(NAME)
	install $(NAME) $(XBARDIR)/$(NAME)

clean:
	rm -f $(NAME)
