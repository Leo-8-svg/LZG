#ifndef COLLECTION_H
#define COLLECTION_H

#include "include/tokenizer.h"

typedef struct{
    const Document *documents;
    int count;
}DocumentCollenct;


int collection_wordcount(const void *data, int doc_id);

const char *collection_filename(const void *data, int doc_id);

#endif