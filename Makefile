CFLAGS = -Wall -Wextra -O2 -g -DDEBUG=0
LDFLAGS := -pthread
CC = gcc
RM = rm -f
MD5 = md5sum
TIME = time

SRC = tinyjpeg.c loadjpeg.c tinyjpeg-parse.c jidctflt.c conv_yuvbgr.c huffman.c timeutil.c chunk_distributor.c
OBJ = $(SRC:.c=.o)
EXEC = tinyjpeg

IMPATH = ../test_images
INPUT = earth-8k


INFILE  = $(IMPATH)/$(INPUT).jpg
OUTFILE = $(INPUT).tga


all: $(EXEC)

$(EXEC): $(OBJ)
	@echo "------------------------------------------------------------------"
	@echo "Building executable"
	@echo "------------------------------------------------------------------"
	$(CC) -o $@ $(LDFLAGS) $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "------------------------------------------------------------------"
	@echo "Cleaning up"
	@echo "------------------------------------------------------------------"
	$(RM) $(EXEC) $(OBJ) *.tga

run: $(EXEC)
	@echo "------------------------------------------------------------------"
	@echo "Run (target $(OUTFILE))"
	@echo "------------------------------------------------------------------"
	$(RM) $(OUTFILE)
	time ./$(EXEC) $(INFILE) $(OUTFILE)

nullrun: $(EXEC)
	@echo "------------------------------------------------------------------"
	@echo "Run (target /dev/null)"
	@echo "------------------------------------------------------------------"
	time ./$(EXEC) $(INFILE) /dev/null
	

$(OUTFILE): run

check: $(OUTFILE)
	@echo "------------------------------------------------------------------"
	@echo "Check"
	@echo "------------------------------------------------------------------"
	$(MD5) --check $(IMPATH)/$(INPUT).md5

both: clean nullrun check
