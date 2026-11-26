#ifndef COMP1028_INDEX_H
#define COMP1028_INDEX_H

#include <stdio.h>

#include "search.h"

#ifndef MAX_WORD
#define MAX_WORD 128
#endif

#define INDEX_WORD_LEN MAX_WORD

typedef struct Document {
    int id;
    char filename[256];
} Document;

int next_word(FILE *file, char *word, int size);

int index_add_occurrence(InvertedIndex *index, const char *word, int doc_id);
int index_build_from_documents(InvertedIndex *index,
                               const Document *docs,
                               int count);
void index_free(InvertedIndex *index);

const PostingNode *index_lookup(const InvertedIndex *index, const char *word);
int index_posting_count(const InvertedIndex *index);
int index_save(const InvertedIndex *index, const char *path);
int index_load(InvertedIndex *index, const char *path);

#endif
