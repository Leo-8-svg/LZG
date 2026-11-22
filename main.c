#include <stdio.h>
#include "include/tokenizer.h"
#include "index.h"
#include "search.h"
#include "collection.h"



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

    const char *name = collection_filename(&collection, 0);

    if (name != NULL) {
        printf("Adapter filename: %s\n", name);
    } else {
        printf("Cannot get filename.\n");
    }

    //to built the index from documents
    InvertedIndex index = {0};

    if (index_build_from_documents(&index, docs, n) != 0) {
        printf("Failed to build index.\n");
        index_free(&index);
        return 1;
    }

    printf("Indexed documents: %d\n", index.document_count);

    printf("Unique terms: %d\n", index.unique_term_count);

    char query[256];

    printf("Enter search query: ");

    if (fgets(query, sizeof(query), stdin) == NULL) {
        printf("Failed to read query.\n");
        index_free(&index);
        return 1;
    }

    printf("Query received: %s", query);

    SearchResponse *response = search_indexed(
        &index, &collection, n, query, 5, SCORE_TERM_FREQUENCY);

    if (response == NULL) {
        printf("Failed to search.\n");
        index_free(&index);
        return 1;
    }

    print_search_results(response, 5);
    free_search_response(response);

    index_free(&index);

    return 0;
}