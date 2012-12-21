CFLAGS = -Wall -Wextra -O2 -g -DDEBUG=0
LDFLAGS := -pthread
CC = gcc
RM = rm -f
MD5 = md5sum
TIME = time

SRC = tinyjpeg.c loadjpeg.c tinyjpeg-parse.c jidctflt.c conv_yuvbgr.c huffman.c timeutil.c
OBJ = $(SRC:.c=.o)
EXEC = tinyjpeg

IMPATH = ../test_images
INPUT = earth-8k


INFILE  = $(IMPATH)/$(INPUT).jpg
OUTFILE = $(INPUT).tga


all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(EXEC) $(OBJ) *.tga

run: $(EXEC)
	echo "Run..."
	$(RM) $(OUTFILE)
	time ./$(EXEC) $(INFILE) $(OUTFILE)

null: $(EXEC)
	time ./$(EXEC) $(INFILE) /dev/null
	

$(OUTFILE): run

check: 
	echo "Checksum..."
	$(MD5) --check $(IMPATH)/$(INPUT).md5
