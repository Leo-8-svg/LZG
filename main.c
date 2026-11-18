#include <stdio.h>
#include "include/tokenizer.h"

int main(void){
    Document docs[MAX_DOCS];

    int n = load_documents("data/demo_documents.txt", docs, MAX_DOCS);

    if (n <= 0) {
        printf("No documents loaded. Check file paths.\n");
        return 1;
    }

    printf("Documents: %d\n", n);
    printf("First file: %s\n", docs[0].filename);
    printf("Word count: %d\n", docs[0].word_count);


    char query[256];

    printf("Enter search query: ");

    if (fgets(query, sizeof(query), stdin) == NULL) {
        printf("Failed to read query.\n");
        return 1;
    }

    printf("Query received: %s", query);

    return 0;
}