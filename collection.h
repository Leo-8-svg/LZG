#ifndef COLLECTION_H
#define COLLECTION_H

#include "include/tokenizer.h"

typedef struct{
    const Document *documents;
    int count;
}DocumentCollenct;


int collection_wordcount(const void *data, int doc_id);

const char *collection_filename(const void *data, int doc_id);

int collection_save_metadata(const DocumentCollenct *collection, const char *path);
int collection_matches_metadata(const DocumentCollenct *collection, const char *path);

#endif