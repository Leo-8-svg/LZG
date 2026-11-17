#include <stdio.h>
#include "tokenizer.h"

int main(int argc, char **argv) {
    const char *list_path = (argc > 1) ? argv[1] : "data/documents.txt";
    Document docs[MAX_DOCS];

    int n = load_documents(list_path, docs, MAX_DOCS);
    if (n < 0) return 1;

    int total = 0;
    for (int i = 0; i < n; i++) {
        printf("doc %d: %s  words=%d\n",
               docs[i].id, docs[i].filename, docs[i].word_count);
        total += docs[i].word_count;
    }
    printf("Total documents: %d, total words: %d\n", n, total);

    return 0;
}