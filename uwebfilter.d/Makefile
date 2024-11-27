
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

install:
	cp $(OUT) /usr/bin/
	nft -f nftrules
	mkdir -p ~/.config/uwebfilter
	cp uwebfilterd.cfg ~/.config/uwebfilter/config
	cp uwebfilterd.service /etc/systemd/system/
	systemctl enable uwebfilterd
	systemctl start uwebfilterd
