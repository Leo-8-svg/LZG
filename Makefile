# COMP1028 - Document Search and Information Retrieval System
#
# Only the files below belong to the final program.  test_index.c and
# test_search.c are separate harnesses, and searchfinal.c / indexfinal.h are
# earlier single-file versions of the index/search modules, so none of them may
# be added to SRC here - doing so links duplicate symbols.

CC      ?= gcc
CFLAGS  ?= -std=c17 -Wall -Wextra -Wpedantic -O2 -I. -Iinclude
LDLIBS  ?= -lm

SRC = main.c collection.c src/tokenizer.c index.c search.c ranking.c report.c ui.c
HDR = include/tokenizer.h index.h search.h collection.h ui.h report.h
OBJ = $(SRC:.c=.o)

search: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDLIBS) -o $@

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean check
clean:
	rm -f $(OBJ) search index.txt index_documents.txt search_results.txt

check: search
	printf 'data/demo.txt\ndata/demo2.txt\n' > data/demo_documents.txt
	rm -f index.txt index_documents.txt
	printf '1\n2\n3\nsolar energy\n5\n0\n' | ./search
