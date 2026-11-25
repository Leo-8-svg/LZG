#include "report.h"

#include <stdio.h>
#include <stdlib.h>

int report_save_results(const SearchResponse *response, const char *path) {
    if (response == NULL || path == NULL || path[0] == '\0') {
        return -1;
    }

    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        return -1;
    }

    fprintf(fp, "=== Search Results Report ===\n");

    

    fprintf(fp, "Total results: %d\n\n", response->returned_count);

    if (response->returned_count <= 0 || response->results == NULL) {
        fprintf(fp, "No matching documents found.\n");
    } else {
        for (int i = 0; i < response->returned_count; i++) {
            fprintf(fp, "%d. Doc ID: %d | File: %s | Score: %.4f\n",
                    i + 1,
                    response->results[i].doc_id,
                    response->results[i].filename,
                    response->results[i].score);
        }
    }

    if (fclose(fp) != 0) {
        return -1;
    }

    return 0;
}
