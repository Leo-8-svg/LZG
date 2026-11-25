#include "collection.h"
#include <stdio.h>
#include <string.h>

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

int collection_save_metadata(const DocumentCollenct *collection, const char *path)
{
    if (collection == NULL || collection->documents == NULL || path == NULL ||
        collection->count <= 0 || collection->count > MAX_DOCS) {
        return -1;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return -1;
    }
    if (fprintf(file, "COMP1028_DOCS 1\n%d\n", collection->count) < 0) {
        fclose(file);
        return -1;
    }

    for (int i = 0; i < collection->count; i++) {
        const Document *document = &collection->documents[i];
        if (document->id != i || document->word_count < 0 ||
            fprintf(file, "%d %d\n%s\n", document->id,
                    document->word_count, document->filename) < 0) {
            fclose(file);
            return -1;
        }
    }
    if (fprintf(file, "END\n") < 0) {
        fclose(file);
        return -1;
    }
    return fclose(file) == 0 ? 0 : -1;
}

int collection_matches_metadata(const DocumentCollenct *collection, const char *path)
{
    if (collection == NULL || collection->documents == NULL || path == NULL ||
        collection->count <= 0 || collection->count > MAX_DOCS) {
        return -1;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    char line[PATH_LEN + 3];
    char extra;
    int count;
    if (fgets(line, sizeof(line), file) == NULL ||
        strcmp(line, "COMP1028_DOCS 1\n") != 0 ||
        fgets(line, sizeof(line), file) == NULL ||
        sscanf(line, "%d %c", &count, &extra) != 1 ||
        count != collection->count) {
        fclose(file);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        int id;
        int word_count;
        if (fgets(line, sizeof(line), file) == NULL ||
            sscanf(line, "%d %d %c", &id, &word_count, &extra) != 2 ||
            id != collection->documents[i].id ||
            word_count != collection->documents[i].word_count ||
            fgets(line, sizeof(line), file) == NULL) {
            fclose(file);
            return -1;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, collection->documents[i].filename) != 0) {
            fclose(file);
            return -1;
        }
    }

    if (fgets(line, sizeof(line), file) == NULL ||
        strcmp(line, "END\n") != 0 || fgetc(file) != EOF || ferror(file)) {
        fclose(file);
        return -1;
    }
    return fclose(file) == 0 ? 0 : -1;
}