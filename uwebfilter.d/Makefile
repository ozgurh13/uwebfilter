CC = gcc
OUT = uwebfilterd
UPX = upx

CFLAGS  = -W -Wall -Wextra -s -Os
CFLAGS += -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-z,norelro
CFLAGS += -Winit-self -Wundef -Wshadow -Wpointer-arith -Wwrite-strings
CFLAGS += -Waggregate-return -Wcast-qual -Wuninitialized -Wno-unused-result
CFLAGS += -Wformat=2 -Wformat-security

LDLIBS  = -lnetfilter_queue -lmnl -lnfnetlink
LDLIBS += -lcurl -ljson-c -lsqlite3

CFILES = src/*.c


all:
	$(CC) $(CFLAGS) $(CFILES) -o $(OUT) $(LDLIBS)
	$(UPX) --best --ultra-brute $(OUT)

clean:
	rm -f $(OUT)

run: all
	./$(OUT)

install:
	cp $(OUT) /usr/bin/
	nft -f nftrules

install-config:
	mkdir -p ~/.config/uwebfilterd
	cp uwebfilterd.cfg ~/.config/uwebfilterd/config

install-service:
	cp uwebfilterd.service /etc/systemd/system/
	systemctl enable uwebfilterd
	systemctl start uwebfilterd
