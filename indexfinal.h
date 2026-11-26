#ifndef COMP1028_INDEX_H
#define COMP1028_INDEX_H

#include <stdio.h>

#ifndef MAX_WORD
#define MAX_WORD 128
#endif

#define INDEX_WORD_LEN MAX_WORD

typedef struct PostingNode {
    int doc_id;
    int term_frequency;
    struct PostingNode *next;
} PostingNode;

typedef struct IndexEntry {
    char *term;
    PostingNode *postings;
    struct IndexEntry *next;
} IndexEntry;

typedef struct InvertedIndex {
    IndexEntry *head;
    int document_count;
    int unique_term_count;
} InvertedIndex;

#define SEARCH_POSTING_DOC_ID(posting) ((posting)->doc_id)
#define SEARCH_POSTING_FREQUENCY(posting) ((posting)->term_frequency)
#define SEARCH_POSTING_NEXT(posting) ((posting)->next)

typedef enum ScoringMethod {
    SCORE_TERM_FREQUENCY = 0,
    SCORE_TF_IDF = 1
} ScoringMethod;

typedef struct SearchResult {
    int doc_id;
    char filename[256];
    double score;
    int matched_term_count;
    char **matched_terms;
} SearchResult;

typedef struct SearchResponse {
    SearchResult *results;
    int total_matches;
    int returned_count;
    double query_latency_ms;
} SearchResponse;

typedef struct LinearScanDocument {
    int doc_id;
    const char *filename;
    const char *text;
    int total_terms;
} LinearScanDocument;

typedef struct SearchAdapterHooks {
    int (*tokenize_query)(const char *raw_query,
                          char ***out_tokens,
                          int *out_count);
    const PostingNode *(*lookup_term)(const InvertedIndex *index,
                                      const char *term);
    const char *(*get_doc_filename)(const void *doc_collection,
                                    int doc_id);
    int (*get_doc_total_terms)(const void *doc_collection,
                               int doc_id);
} SearchAdapterHooks;

typedef struct Document {
    int id;
    char filename[256];
} Document;

int next_word(FILE *file, char *word, int size);

void search_set_adapter_hooks(const SearchAdapterHooks *hooks);
void search_clear_adapter_hooks(void);

int adapter_tokenize_query(const char *raw_query,
                           char ***out_tokens,
                           int *out_count);
const PostingNode *adapter_lookup_term(const InvertedIndex *index,
                                       const char *term);
const char *adapter_get_doc_filename(const void *doc_collection,
                                     int doc_id);
int adapter_get_doc_total_terms(const void *doc_collection,
                                int doc_id);

SearchResponse *search_indexed(const InvertedIndex *index,
                               const void *doc_collection,
                               int document_count,
                               const char *raw_query,
                               int top_k,
                               ScoringMethod scoring_method);
SearchResponse *search_linear_scan(const LinearScanDocument *documents,
                                   int document_count,
                                   const char *raw_query,
                                   int top_k,
                                   ScoringMethod scoring_method);
void free_search_response(SearchResponse *response);

double ranking_term_frequency_score(int term_frequency,
                                    int query_multiplicity);
double ranking_tf_idf_score(int term_frequency,
                            int document_frequency,
                            int collection_document_count,
                            int query_multiplicity);
int sort_search_results(SearchResponse *response);
int truncate_search_response(SearchResponse *response, int top_k);
void print_search_results(const SearchResponse *response, int top_k);

int index_add_occurrence(InvertedIndex *index, const char *word, int doc_id);
int index_build_from_documents(InvertedIndex *index,
                               const Document *docs,
                               int count);
void index_free(InvertedIndex *index);
const PostingNode *index_lookup(const InvertedIndex *index, const char *word);
int index_posting_count(const InvertedIndex *index);
int index_save(const InvertedIndex *index, const char *path);
int index_load(InvertedIndex *index, const char *path);

#endif
