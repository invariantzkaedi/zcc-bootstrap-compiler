CC = gcc
ZCC = ../zcc
ZCC2 = ../zcc2
ZCC3 = ../zcc3

CFLAGS_BOOT = -c
CFLAGS_KMAIN = -emit-obj
LDFLAGS = -T linker.ld

.PHONY: all clean stage2 stage3 test verify

all: zkernel.elf

boot.o: boot.S
	$(CC) $(CFLAGS_BOOT) boot.S -o boot.o

kmain.o: kmain.c
	$(ZCC) $(CFLAGS_KMAIN) kmain.c -o kmain.o

zkernel.elf: boot.o kmain.o linker.ld
	ld -T linker.ld -o zkernel.elf boot.o kmain.o

stage2: boot.o kmain.c linker.ld
	$(ZCC2) $(CFLAGS_KMAIN) kmain.c -o kmain2.o
	$(ZCC2) -zld -o zkernel-stage2.elf boot.o kmain2.o -T linker.ld

stage3: boot.o kmain.c linker.ld
	$(ZCC3) $(CFLAGS_KMAIN) kmain.c -o kmain3.o
	$(ZCC3) -zld -o zkernel-stage3.elf boot.o kmain3.o -T linker.ld

test: zkernel.elf
	python3 verify_kernel.py zkernel.elf

verify: stage2 stage3
	@echo "=== Verifying Stage 2 zkernel ==="
	python3 verify_kernel.py zkernel-stage2.elf
	@echo "=== Verifying Stage 3 zkernel ==="
	python3 verify_kernel.py zkernel-stage3.elf
	@echo "=== Boot Parity Verification Successful ==="

clean:
	rm -f *.o *.elf

include Makefile.zld
