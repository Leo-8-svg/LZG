#include <stdio.h>
#include "include/tokenizer.h"
#include "index.h"
#include "search.h"
#include "collection.h"
#include "ui.h"
#include "report.h"


int main(void){
    Document docs[MAX_DOCS];
    int n = 0;
    DocumentCollenct collection = {docs, 0};
    InvertedIndex index = {0};
    int index_ready = 0;
    SearchResponse *response = NULL;

    while (1) {
        int choice;
        int status = ui_read_menu(&choice);

        if (status == 0) {
            break;
        }
        if (status < 0) {
            printf("Invalid menu choice. Please try again.\n");
            continue;
        }
        if (choice == 0) {
            break;
        }

        if (choice == 1) {
            free_search_response(response);
            response = NULL;
            index_free(&index);
            index_ready = 0;

            n = load_documents("data/demo_documents.txt", docs, MAX_DOCS);
            if (n < 0) {
                n = 0;
            }
            collection.count = n;

            if (n == 0) {
                printf("No documents loaded. Check data/demo_documents.txt.\n");
            } else {
                printf("Loaded documents: %d. Build or load an index next.\n", n);
            }

        } else if (choice == 2) {
            if (n == 0) {
                printf("Please load documents first.\n");
                continue;
            }

            free_search_response(response);
            response = NULL;
            index_ready = 0;

            if (index_build_from_documents(&index, docs, n) != 0) {
                printf("Failed to build index.\n");
                continue;
            }
            index_ready = 1;
            printf("Index built. Documents: %d; unique terms: %d\n",
                   index.document_count, index.unique_term_count);

        } else if (choice == 3) {
            if (!index_ready) {
                printf("Please build or load an index first.\n");
                continue;
            }
            char query[256];
            int top_k;

            status = ui_read_query(query, sizeof(query));
            if (status == 0) {
                break;
            }
            if (status < 0) {
                printf("Invalid query. Please try again.\n");
                continue;
            }

            status = ui_read_top_k(&top_k);
            if (status == 0) {
                break;
            }
            if (status < 0) {
                printf("Invalid Top K. Please try again.\n");
                continue;
            }

            SearchResponse *new_response = search_indexed(
                &index, &collection, n, query, top_k,
                SCORE_TERM_FREQUENCY);

            if (new_response == NULL) {
                printf("Failed to search.\n");
                continue;
            }

            free_search_response(response);
            response = new_response;
            print_search_results(response, top_k);

        } else if (choice == 4) {
            ui_show_statistics(&collection, &index);

        } else if (choice == 5) {
            if (response == NULL) {
                printf("Please run a search first.\n");
            } else if (report_save_results(
                           response, "search_results.txt") == 0) {
                printf("Report saved to search_results.txt\n");
            } else {
                printf("Failed to save report.\n");
            }

        }  else if (choice == 6) {
            if (!index_ready) {
                printf("Please build or load an index first.\n");
                continue;
            }

            // Invalidate the old mapping 
            FILE *metadata = fopen("index_documents.txt", "w");
            if (metadata == NULL) {
                printf("Cannot create index_documents.txt.\n");
                continue;
            }
            if (fclose(metadata) != 0 ||
                index_save(&index, "index.txt") != 0 ||
                collection_save_metadata(&collection, "index_documents.txt") != 0) {
                printf("Failed to save index and document metadata.\n");
                continue;
            }
            printf("Saved index.txt and index_documents.txt. Keep both files together.\n");

        } else if (choice == 7) {
            if (n == 0) {
                printf("Please load documents first.\n");
                continue;
            }
            if (collection_matches_metadata(&collection, "index_documents.txt") != 0) {
                printf("Saved document metadata is missing or does not match.\n");
                printf("Use the original document list, or build and save a new index.\n");
                continue;
            }

            InvertedIndex loaded_index = {0};
            if (index_load(&loaded_index, "index.txt") != 0) {
                index_free(&loaded_index);
                printf("Failed to load index.\n");
                continue;
            }
            if (loaded_index.document_count != n) {
                index_free(&loaded_index);
                printf("Saved index document count does not match.\n");
                continue;
            }

            free_search_response(response);
            response = NULL;
            index_free(&index);
            index = loaded_index;
            index_ready = 1;
            printf("Index loaded. Documents: %d; unique terms: %d\n",
                   index.document_count, index.unique_term_count);
        }
    }

    free_search_response(response);
    index_free(&index);
    return 0;
}