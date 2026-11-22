#include "collection.h"

int collection_wordcount(const void *data, int doc_id){

    const DocumentCollenct *collection=data;

    if(collection== NULL|| collection->documents== NULL||doc_id<0||doc_id>=collection->count){
        return -1;
    }
    return collection->documents[doc_id].word_count;

}

const char *collection_filename(const void *data, int doc_id)
{
    const DocumentCollenct *collection = data;

    if (collection == NULL ||
        collection->documents == NULL ||
        doc_id < 0 ||
        doc_id >= collection->count) {
        return NULL;
    }

    return collection->documents[doc_id].filename;
}