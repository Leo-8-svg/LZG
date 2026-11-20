#ifndef COMP1028_INDEX_H
#define COMP1028_INDEX_H

#include <stdio.h>
#include "include/tokenizer.h"

#define INDEX_WORD_LEN 128

typedef struct PostingNode {
    int doc_id;
    int term_frequency;
    struct PostingNode *next;
} PostingNode;

typedef struct IndexEntry {
    char *term;
    PostingNode *postings;
    struct IndexEntry *next;
} IndexEntry;

typedef struct InvertedIndex {
    IndexEntry *head;
    int document_count;
    int unique_term_count;
} InvertedIndex;




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
