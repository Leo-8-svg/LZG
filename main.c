#include <stdio.h>
#include "include/tokenizer.h"

/*Array and numbers of documents to store*/
typedef struct{
    const Document *documents; //Pointer to ducument array
    int count;
} 
DocumentCollenct;

int collection_wordcount(const void *data, int doc_id){

    const DocumentCollenct *collection=data;

    if(collection== NULL|| collection->documents== NULL||doc_id<0||doc_id>=collection->count){
        return -1;
    }
    return collection->documents[doc_id].word_count;

}

int main(void){
    Document docs[MAX_DOCS];

    int n = load_documents("data/demo_documents.txt", docs, MAX_DOCS);

    if (n <= 0) {
        printf("No documents loaded. Check file paths.\n");
        return 1;
    }

    printf("The Documents: %d\n", n);

    printf("The First file: %s\n", docs[0].filename);

    printf("Word count: %d\n", docs[0].word_count);

    DocumentCollenct collection= {docs, n};
    printf("Adapter word count: %d\n", collection_wordcount(&collection, 0));



    char query[256];

    printf("Enter search query: ");

    if (fgets(query, sizeof(query), stdin) == NULL) {
        printf("Failed to read query.\n");
        return 1;
    }

    printf("Query received: %s", query);

    return 0;
}