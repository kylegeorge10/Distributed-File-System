#
# CMPSC311 - F21 Assignment #3
# Makefile - makefile for the assignment
#

# Make environment
INCLUDES=-I.
CC=./311cc
CFLAGS=-I. -c -g -Wall $(INCLUDES)
LINKARGS=-g
LIBS=-lm -lcmpsc311 -L. -lgcrypt -lpthread -lcurl
                    
# Suffix rules
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS)  -o $@ $<
	
# Files
OBJECT_FILES=	fs3_sim.o \
				fs3_driver.o \
				fs3_cache.o \
				fs3_network.o \
				fs3_common.o \

# Productions
all : fs3_client

fs3_client : $(OBJECT_FILES)
	$(CC) $(LINKARGS) $(OBJECT_FILES) -o $@ $(LIBS)

clean : 
	rm -f fs3_client $(OBJECT_FILES)
	
test: fs3_client 
	./fs3_client -v assign4-small-workload.txt
