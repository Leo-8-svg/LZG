#ifndef COMP1028_SEARCH_H
#define COMP1028_SEARCH_H

/*
 * COMP1028 - Query Processing, Search and Ranking module
 * ------------------------------------------------------
 * This public header deliberately keeps the search/ranking pipeline away
 * from the concrete data structures owned by the other team members.  The
 * four adapter functions below are the only integration points required by
 * the core implementation.
 *
 * Integration note:
 *   The local PostingNode/InvertedIndex definitions are used unless the
 *   build defines EXTERNAL_TYPES_DEFINED.  In that case, the external index
 *   header must be included first and must provide compatible typedef names:
 *       PostingNode and InvertedIndex
 *
 *   The adapter bodies in search.c are the place to connect the teammate's
 *   actual tokenizer and inverted-index APIs.  For a temporary integration
 *   layer, search_set_adapter_hooks() can also be used to inject callbacks.
 */

#ifndef EXTERNAL_TYPES_DEFINED

/*
 * A posting is one term -> one document relationship.  Posting lists are
 * singly linked because the course recommends this representation and
 * because it makes the adapter boundary easy to replace later.
 */
typedef struct PostingNode {
    int doc_id;
    int term_frequency;
    struct PostingNode *next;
} PostingNode;

/* One term and its posting-list head. */
typedef struct IndexEntry {
    char *term;
    PostingNode *postings;
    struct IndexEntry *next;
} IndexEntry;

/*
 * Local/default inverted index.  document_count is the collection size N,
 * while unique_term_count is useful for diagnostics and indexing reports.
 */
typedef struct InvertedIndex {
    IndexEntry *head;
    int document_count;
    int unique_term_count;
} InvertedIndex;

#endif /* EXTERNAL_TYPES_DEFINED */

/*
 * Posting field-access macros.
 *
 * These defaults match the local PostingNode above.  If the team's posting
 * struct uses names such as "document_id", "frequency" or "link", define the
 * three macros to the corresponding expressions before search.c is compiled.
 * Keeping these accesses here avoids editing the search/ranking algorithms.
 */
#ifndef SEARCH_POSTING_DOC_ID
#define SEARCH_POSTING_DOC_ID(posting) ((posting)->doc_id)
#endif

#ifndef SEARCH_POSTING_FREQUENCY
#define SEARCH_POSTING_FREQUENCY(posting) ((posting)->term_frequency)
#endif

#ifndef SEARCH_POSTING_NEXT
#define SEARCH_POSTING_NEXT(posting) ((posting)->next)
#endif

typedef enum ScoringMethod {
    SCORE_TERM_FREQUENCY = 0,
    SCORE_TF_IDF = 1
} ScoringMethod;

/*
 * One final ranked document.
 *
 * matched_terms is a deep-owned array of separately allocated C strings.
 * A term appears at most once in this array even if it was repeated in the
 * raw query.  Repeated query terms still contribute additional weight to
 * the numeric score, as documented in search.c.
 */
typedef struct SearchResult {
    int doc_id;
    char filename[256];
    double score;
    int matched_term_count;
    char **matched_terms;
} SearchResult;

/*
 * Query response.  total_matches is the number of matching documents before
 * Top-K truncation; returned_count is the number actually returned.
 */
typedef struct SearchResponse {
    SearchResult *results;
    int total_matches;
    int returned_count;
    double query_latency_ms;
} SearchResponse;

/*
 * Read-only input for the benchmark baseline.  This view contains the
 * original text and is intentionally independent of the inverted index.
 * It lets the baseline scan raw documents without depending on Member C's
 * internal collection representation.
 */
typedef struct LinearScanDocument {
    int doc_id;
    const char *filename;
    const char *text;
    int total_terms;
} LinearScanDocument;

/*
 * Optional adapter hooks.
 *
 * Tokenizer ownership contract:
 *   - tokenize_query allocates *out_tokens as an array of malloc-owned
 *     strings and returns 0 on success, -1 on allocation failure.
 *   - The caller (search.c) frees both each string and the array.
 *   - Tokens must already be normalized to lowercase.
 *
 * The hook mechanism is useful for unit tests and for a thin temporary
 * bridge during team integration.  It is not a replacement for the four
 * adapter functions when the external structures change.
 */
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

/* Install/clear optional adapter callbacks.  Passing NULL clears them. */
void search_set_adapter_hooks(const SearchAdapterHooks *hooks);
void search_clear_adapter_hooks(void);

/*
 * The four required adapter functions.  Their implementations live in
 * search.c and are the only places that should need changes when the
 * teammates' tokenizer/index/document structures are finalized.
 */
int adapter_tokenize_query(const char *raw_query,
                           char ***out_tokens,
                           int *out_count);
const PostingNode *adapter_lookup_term(const InvertedIndex *index,
                                       const char *term);
const char *adapter_get_doc_filename(const void *doc_collection,
                                     int doc_id);
int adapter_get_doc_total_terms(const void *doc_collection,
                                int doc_id);

/*
 * Indexed search.
 *
 * document_count is N for TF-IDF and is supplied by the collection layer;
 * the query core therefore does not read a potentially changing index field.
 * Returns a valid empty response for invalid/null input.  Returns NULL only
 * when the response object itself cannot be allocated.
 */
SearchResponse *search_indexed(const InvertedIndex *index,
                               const void *doc_collection,
                               int document_count,
                               const char *raw_query,
                               int top_k,
                               ScoringMethod scoring_method);

/*
 * Linear-scan benchmark baseline.
 *
 * It tokenizes and scans original text directly.  It does not consult the
 * inverted index.  The two-pass implementation first computes document
 * frequencies for TF-IDF and then scores matching documents.
 */
SearchResponse *search_linear_scan(const LinearScanDocument *documents,
                                   int document_count,
                                   const char *raw_query,
                                   int top_k,
                                   ScoringMethod scoring_method);

/* Deep release of all strings, matched-term arrays, results and response. */
void free_search_response(SearchResponse *response);

/*
 * Ranking and sorting API.
 *
 * The score functions are public so unit tests and reports can document the
 * exact formulas.  sort_search_results() uses a hand-written stable merge
 * sort with deterministic tie-breaking.
 */
double ranking_term_frequency_score(int term_frequency,
                                    int query_multiplicity);
double ranking_tf_idf_score(int term_frequency,
                            int document_frequency,
                            int collection_document_count,
                            int query_multiplicity);

int sort_search_results(SearchResponse *response);
int truncate_search_response(SearchResponse *response, int top_k);
void print_search_results(const SearchResponse *response, int top_k);

/*
 * Retrieval-quality metrics.
 *
 * Precision@K uses K as its denominator, even when fewer than K results are
 * returned.  Recall@K uses the number of unique relevant document IDs as its
 * denominator.  calculate_reciprocal_rank() returns 1/rank of the first
 * relevant result and 0 if no relevant result is present.  Average this value
 * over queries to obtain MRR.
 */
double calculate_precision_at_k(const SearchResponse *response,
                                const int *relevant_doc_ids,
                                int num_relevant,
                                int k);
double calculate_recall_at_k(const SearchResponse *response,
                             const int *relevant_doc_ids,
                             int num_relevant,
                             int k);
double calculate_reciprocal_rank(const SearchResponse *response,
                                 const int *relevant_doc_ids,
                                 int num_relevant);

#endif /* COMP1028_SEARCH_H */
