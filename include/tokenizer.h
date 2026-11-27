#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdio.h>

#define MAX_WORD 128
#define MAX_DOCS 256
#define PATH_LEN 256

typedef struct {
    int  id;
    char filename[PATH_LEN];
    int  word_count;
} Document;

int next_word(FILE *input, char *word, int capacity);
int load_documents(const char *list_path, Document *docs, int max_docs);

#endif