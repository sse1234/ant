# ant-server build. Two toolchain options — neither installed yet, see
# README "Toolchain". Targets AmigaOS 3.x / 68060.

# --- Option 1: bebbo amiga-gcc (native install or inside Docker) ----------
CC      = m68k-amigaos-gcc
CFLAGS  = -mcpu=68060 -O2 -fomit-frame-pointer -noixemul -Wall
TARGET  = ant-server

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

# Run the same build inside the crosstools image (Apple Silicon: runs under
# Rosetta; first pull is slow).
docker:
	docker run --rm -v $(CURDIR):/work -w /work \
		amigadev/crosstools:m68k-amigaos make

# --- Option 2: vbcc -------------------------------------------------------
# vc +aos68k -cpu=68060 -O2 -o ant-server main.c
# (needs vbcc m68k target + NDK3.2 includes incl. netinclude in vbcc config)

clean:
	rm -f $(TARGET) *.o

.PHONY: all docker clean
