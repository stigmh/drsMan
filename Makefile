PROGRAM=drsMan

all: $(PROGRAM)

$(PROGRAM): drs/FileManager.c drs/DRSFormat.c drs/Main.c
	$(CC) -o $@ $^ $(CFLAGS)
	chmod +x $@

clean:
	rm -rf *.o drs/*.o *.dSYM $(PROGRAM)
