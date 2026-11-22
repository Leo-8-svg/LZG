#include "search.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "collection.h"

/*
 * ------------------------- Adapter layer -------------------------
 *
 * The following four functions are deliberately thin.  During integration,
 * replace their bodies with calls to the final team APIs.  No search or
 * ranking code below depends directly on those APIs.
 */

static SearchAdapterHooks g_adapter_hooks;

void search_set_adapter_hooks(const SearchAdapterHooks *hooks)
{
    if (hooks == NULL) {
        memset(&g_adapter_hooks, 0, sizeof(g_adapter_hooks));
    } else {
        g_adapter_hooks = *hooks;
    }
}

void search_clear_adapter_hooks(void)
{
    memset(&g_adapter_hooks, 0, sizeof(g_adapter_hooks));
}

static int ascii_is_token_char(unsigned char c)
{
    return (c >= (unsigned char)'a' && c <= (unsigned char)'z') ||
           (c >= (unsigned char)'A' && c <= (unsigned char)'Z') ||
           (c >= (unsigned char)'0' && c <= (unsigned char)'9');
}

static unsigned char ascii_to_lower(unsigned char c)
{
    if (c >= (unsigned char)'A' && c <= (unsigned char)'Z') {
        return (unsigned char)(c - (unsigned char)'A' + (unsigned char)'a');
    }
    return c;
}

static void free_token_array(char **tokens, int count)
{
    int i;

    if (tokens == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

/*
 * Default query tokenizer used by the standalone module.  It is ASCII-only
 * and therefore has deterministic behavior independent of the process
 * locale.  Non-alphanumeric characters are separators.
 */
static int default_tokenize_query(const char *raw_query,
                                  char ***out_tokens,
                                  int *out_count)
{
    char **tokens;
    int count;
    int capacity;
    size_t i;
    size_t length;

    if (out_tokens != NULL) {
        *out_tokens = NULL;
    }
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (out_tokens == NULL || out_count == NULL || raw_query == NULL) {
        return 0;
    }

    tokens = NULL;
    count = 0;
    capacity = 0;
    length = strlen(raw_query);
    i = 0;

    while (i < length) {
        size_t start;
        size_t token_length;
        char *token;
        char **grown;
        size_t j;

        while (i < length &&
               !ascii_is_token_char((unsigned char)raw_query[i])) {
            i++;
        }
        if (i == length) {
            break;
        }

        start = i;
        while (i < length &&
               ascii_is_token_char((unsigned char)raw_query[i])) {
            i++;
        }
        token_length = i - start;
        token = (char *)malloc(token_length + 1U);
        if (token == NULL) {
            free_token_array(tokens, count);
            return -1;
        }

        for (j = 0; j < token_length; j++) {
            token[j] = (char)ascii_to_lower(
                (unsigned char)raw_query[start + j]);
        }
        token[token_length] = '\0';

        if (count == capacity) {
            int new_capacity = (capacity == 0) ? 4 : capacity * 2;
            if (new_capacity <= capacity) {
                free(token);
                free_token_array(tokens, count);
                return -1;
            }
            grown = (char **)realloc(
                tokens, (size_t)new_capacity * sizeof(*tokens));
            if (grown == NULL) {
                free(token);
                free_token_array(tokens, count);
                return -1;
            }
            tokens = grown;
            capacity = new_capacity;
        }
        tokens[count++] = token;
    }

    *out_tokens = tokens;
    *out_count = count;
    return 0;
}

int adapter_tokenize_query(const char *raw_query,
                           char ***out_tokens,
                           int *out_count)
{
    if (out_tokens != NULL) {
        *out_tokens = NULL;
    }
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (raw_query == NULL || out_tokens == NULL || out_count == NULL) {
        return 0;
    }

    if (g_adapter_hooks.tokenize_query != NULL) {
        return g_adapter_hooks.tokenize_query(raw_query, out_tokens, out_count);
    }
    return default_tokenize_query(raw_query, out_tokens, out_count);
}

const PostingNode *adapter_lookup_term(const InvertedIndex *index,
                                       const char *term)
{
    if (index == NULL || term == NULL) {
        return NULL;
    }
    if (g_adapter_hooks.lookup_term != NULL) {
        return g_adapter_hooks.lookup_term(index, term);
    }

#ifndef EXTERNAL_TYPES_DEFINED
    {
        const IndexEntry *entry;

        for (entry = index->head; entry != NULL; entry = entry->next) {
            if (entry->term != NULL && strcmp(entry->term, term) == 0) {
                return entry->postings;
            }
        }
    }
#endif

    return NULL;
}

const char *adapter_get_doc_filename(const void *doc_collection,
                                     int doc_id)
{
    if (g_adapter_hooks.get_doc_filename != NULL) {
        return g_adapter_hooks.get_doc_filename(doc_collection, doc_id);
    }

    /*
     * No generic filename representation exists until Member C's final
     * collection type is known.  The integration implementation should
     * replace this return with the team collection lookup.
     */
    return collection_filename(doc_collection, doc_id);
}

int adapter_get_doc_total_terms(const void *doc_collection,
                                int doc_id)
{
    if (g_adapter_hooks.get_doc_total_terms != NULL) {
        return g_adapter_hooks.get_doc_total_terms(doc_collection, doc_id);
    }

    /* See adapter_get_doc_filename(); install the final collection adapter. */
    return collection_wordcount(doc_collection,doc_id);
}

/*
 * ------------------------- Utility helpers -------------------------
 */

static char *copy_string(const char *source)
{
    size_t length;
    char *copy;

    if (source == NULL) {
        return NULL;
    }
    length = strlen(source);
    copy = (char *)malloc(length + 1U);
    if (copy != NULL) {
        memcpy(copy, source, length + 1U);
    }
    return copy;
}

static void copy_filename(char destination[256], const char *source)
{
    const char *fallback = "<unknown>";
    const char *value = (source != NULL && source[0] != '\0')
                            ? source
                            : fallback;
    size_t length;

    length = strlen(value);
    if (length >= 256U) {
        length = 255U;
    }
    memcpy(destination, value, length);
    destination[length] = '\0';
}

static SearchResponse *create_empty_response(void)
{
    SearchResponse *response;

    response = (SearchResponse *)calloc(1U, sizeof(*response));
    return response;
}

typedef struct QueryTerm {
    char *text;
    int multiplicity;          /* Number of times it appeared in raw query. */
    int document_frequency;    /* Used by the linear TF-IDF baseline. */
} QueryTerm;

/*
 * Deduplicate normalized query terms while preserving first-occurrence
 * order.  The multiplicity of a duplicate is retained for scoring.  One
 * copy of each unique term is transferred into the QueryTerm array; all
 * other allocated token strings are released here.
 */
static int prepare_query_terms(char **tokens,
                               int token_count,
                               QueryTerm **out_terms,
                               int *out_term_count)
{
    QueryTerm *terms;
    int unique_count;
    int i;

    if (out_terms == NULL || out_term_count == NULL) {
        free_token_array(tokens, token_count);
        return -1;
    }
    *out_terms = NULL;
    *out_term_count = 0;

    if (tokens == NULL || token_count <= 0) {
        free_token_array(tokens, token_count);
        return 0;
    }

    terms = (QueryTerm *)calloc((size_t)token_count, sizeof(*terms));
    if (terms == NULL) {
        free_token_array(tokens, token_count);
        return -1;
    }

    unique_count = 0;
    for (i = 0; i < token_count; i++) {
        int j;
        int found;

        if (tokens[i] == NULL || tokens[i][0] == '\0') {
            free(tokens[i]);
            tokens[i] = NULL;
            continue;
        }

        found = -1;
        for (j = 0; j < unique_count; j++) {
            if (strcmp(terms[j].text, tokens[i]) == 0) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            terms[found].multiplicity++;
            free(tokens[i]);
            tokens[i] = NULL;
        } else {
            terms[unique_count].text = tokens[i];
            terms[unique_count].multiplicity = 1;
            terms[unique_count].document_frequency = 0;
            tokens[i] = NULL;
            unique_count++;
        }
    }
    free(tokens);

    if (unique_count == 0) {
        free(terms);
        terms = NULL;
    }

    *out_terms = terms;
    *out_term_count = unique_count;
    return 0;
}

static void free_query_terms(QueryTerm *terms, int count)
{
    int i;

    if (terms == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        free(terms[i].text);
    }
    free(terms);
}

static int prepare_query(const char *raw_query,
                         QueryTerm **out_terms,
                         int *out_term_count)
{
    char **tokens;
    int token_count;
    int tokenizer_result;

    tokens = NULL;
    token_count = 0;
    tokenizer_result = adapter_tokenize_query(
        raw_query, &tokens, &token_count);
    if (tokenizer_result < 0) {
        free_token_array(tokens, token_count > 0 ? token_count : 0);
        return -1;
    }
    return prepare_query_terms(tokens, token_count, out_terms, out_term_count);
}

/*
 * --------------------- Indexed result accumulator -------------------
 *
 * The inverted index lookup yields postings grouped by term.  Documents are
 * accumulated by doc_id in an open-addressed hash table, so the search does
 * not scan original document text and does not perform a nested posting scan.
 */

typedef struct DocAccumulator {
    int occupied;
    int doc_id;
    double score;
    char **matched_terms;
    int matched_term_count;
    int matched_term_capacity;
} DocAccumulator;

typedef struct ResultTable {
    DocAccumulator *slots;
    size_t capacity;  /* Always a power of two. */
    size_t size;
} ResultTable;

static size_t hash_doc_id(int doc_id, size_t capacity)
{
    unsigned int value = (unsigned int)doc_id;
    value *= 2654435761U;
    return (size_t)value & (capacity - 1U);
}

static void free_accumulator_terms(DocAccumulator *slot)
{
    int i;

    if (slot == NULL) {
        return;
    }
    for (i = 0; i < slot->matched_term_count; i++) {
        free(slot->matched_terms[i]);
    }
    free(slot->matched_terms);
    slot->matched_terms = NULL;
    slot->matched_term_count = 0;
    slot->matched_term_capacity = 0;
}

static void free_result_table(ResultTable *table)
{
    size_t i;

    if (table == NULL) {
        return;
    }
    if (table->slots != NULL) {
        for (i = 0; i < table->capacity; i++) {
            if (table->slots[i].occupied) {
                free_accumulator_terms(&table->slots[i]);
            }
        }
    }
    free(table->slots);
    memset(table, 0, sizeof(*table));
}

static int result_table_init(ResultTable *table)
{
    if (table == NULL) {
        return -1;
    }
    memset(table, 0, sizeof(*table));
    table->capacity = 16U;
    table->slots = (DocAccumulator *)calloc(
        table->capacity, sizeof(*table->slots));
    if (table->slots == NULL) {
        table->capacity = 0U;
        return -1;
    }
    return 0;
}

static int result_table_rehash(ResultTable *table, size_t new_capacity)
{
    DocAccumulator *new_slots;
    size_t i;

    if (table == NULL || new_capacity < 2U) {
        return -1;
    }
    new_slots = (DocAccumulator *)calloc(
        new_capacity, sizeof(*new_slots));
    if (new_slots == NULL) {
        return -1;
    }

    for (i = 0; i < table->capacity; i++) {
        if (table->slots[i].occupied) {
            size_t position = hash_doc_id(
                table->slots[i].doc_id, new_capacity);
            while (new_slots[position].occupied) {
                position = (position + 1U) & (new_capacity - 1U);
            }
            new_slots[position] = table->slots[i];
        }
    }

    free(table->slots);
    table->slots = new_slots;
    table->capacity = new_capacity;
    return 0;
}

static DocAccumulator *result_table_get_or_insert(
    ResultTable *table, int doc_id)
{
    size_t position;
    size_t start;

    if (table == NULL || table->slots == NULL || table->capacity == 0U) {
        return NULL;
    }

    if ((table->size + 1U) * 10U >= table->capacity * 7U) {
        size_t new_capacity;

        if (table->capacity > ((size_t)-1) / 2U) {
            return NULL;
        }
        new_capacity = table->capacity * 2U;
        if (result_table_rehash(table, new_capacity) != 0) {
            return NULL;
        }
    }

    position = hash_doc_id(doc_id, table->capacity);
    start = position;
    do {
        DocAccumulator *slot = &table->slots[position];

        if (!slot->occupied) {
            memset(slot, 0, sizeof(*slot));
            slot->occupied = 1;
            slot->doc_id = doc_id;
            table->size++;
            return slot;
        }
        if (slot->doc_id == doc_id) {
            return slot;
        }
        position = (position + 1U) & (table->capacity - 1U);
    } while (position != start);

    return NULL;
}

static int accumulator_add_matched_term(DocAccumulator *slot,
                                        const char *term)
{
    int i;
    char *copy;
    char **grown;
    int new_capacity;

    if (slot == NULL || term == NULL) {
        return -1;
    }
    for (i = 0; i < slot->matched_term_count; i++) {
        if (strcmp(slot->matched_terms[i], term) == 0) {
            return 0;
        }
    }

    if (slot->matched_term_count == slot->matched_term_capacity) {
        new_capacity = (slot->matched_term_capacity == 0)
                           ? 2
                           : slot->matched_term_capacity * 2;
        if (new_capacity <= slot->matched_term_capacity) {
            return -1;
        }
        grown = (char **)realloc(
            slot->matched_terms,
            (size_t)new_capacity * sizeof(*slot->matched_terms));
        if (grown == NULL) {
            return -1;
        }
        slot->matched_terms = grown;
        slot->matched_term_capacity = new_capacity;
    }

    copy = copy_string(term);
    if (copy == NULL) {
        return -1;
    }
    slot->matched_terms[slot->matched_term_count++] = copy;
    return 0;
}

static int count_postings(const PostingNode *posting)
{
    int count = 0;

    while (posting != NULL) {
        if (SEARCH_POSTING_DOC_ID(posting) >= 0 &&
            SEARCH_POSTING_FREQUENCY(posting) > 0) {
            if (count < 2147483647) {
                count++;
            }
        }
        posting = SEARCH_POSTING_NEXT(posting);
    }
    return count;
}

static double score_term(const QueryTerm *term,
                         const PostingNode *posting,
                         int document_count,
                         ScoringMethod scoring_method)
{
    if (term == NULL || posting == NULL ||
        SEARCH_POSTING_FREQUENCY(posting) <= 0) {
        return 0.0;
    }
    if (scoring_method == SCORE_TF_IDF) {
        return ranking_tf_idf_score(
            SEARCH_POSTING_FREQUENCY(posting),
            term->document_frequency,
            document_count,
            term->multiplicity);
    }
    return ranking_term_frequency_score(
        SEARCH_POSTING_FREQUENCY(posting), term->multiplicity);
}

/*
 * ------------------------- Indexed search -------------------------
 */

SearchResponse *search_indexed(const InvertedIndex *index,
                               const void *doc_collection,
                               int document_count,
                               const char *raw_query,
                               int top_k,
                               ScoringMethod scoring_method)
{
    SearchResponse *response;
    QueryTerm *terms;
    int term_count;
    ResultTable table;
    clock_t start_clock;
    int i;
    int successful;

    response = create_empty_response();
    if (response == NULL) {
        return NULL;
    }

    start_clock = clock();
    terms = NULL;
    term_count = 0;
    memset(&table, 0, sizeof(table));
    successful = 1;

    if (index == NULL || raw_query == NULL || top_k < 0 ||
        (scoring_method != SCORE_TF_IDF &&
         scoring_method != SCORE_TERM_FREQUENCY)) {
        successful = 0;
    }

    if (successful &&
        prepare_query(raw_query, &terms, &term_count) != 0) {
        successful = 0;
    }

    if (successful && terms != NULL && term_count > 0) {
        if (result_table_init(&table) != 0) {
            successful = 0;
        }
    }

    if (successful && terms != NULL && term_count > 0) {
        for (i = 0; i < term_count; i++) {
            const PostingNode *posting;

            posting = adapter_lookup_term(index, terms[i].text);
            terms[i].document_frequency = count_postings(posting);

            while (posting != NULL) {
                DocAccumulator *slot;
                int total_terms;

                if (SEARCH_POSTING_DOC_ID(posting) < 0 ||
                    SEARCH_POSTING_FREQUENCY(posting) <= 0) {
                    posting = SEARCH_POSTING_NEXT(posting);
                    continue;
                }

                /*
                 * Metadata is validated through the adapter boundary.  The
                 * current course formulas use raw TF, so total_terms is not a
                 * divisor here; it remains available for a normalized-TF
                 * extension and catches malformed external metadata early.
                 */
                total_terms = adapter_get_doc_total_terms(
                    doc_collection, SEARCH_POSTING_DOC_ID(posting));
                if (total_terms < 0) {
                    posting = SEARCH_POSTING_NEXT(posting);
                    continue;
                }

                slot = result_table_get_or_insert(
                    &table, SEARCH_POSTING_DOC_ID(posting));
                if (slot == NULL ||
                    accumulator_add_matched_term(slot, terms[i].text) != 0) {
                    successful = 0;
                    break;
                }
                slot->score += score_term(
                    &terms[i], posting, document_count, scoring_method);
                posting = SEARCH_POSTING_NEXT(posting);
            }
            if (!successful) {
                break;
            }
        }
    }

    if (successful && table.size > 0U) {
        size_t slot_index;
        int result_index;

        response->results = (SearchResult *)calloc(
            table.size, sizeof(*response->results));
        if (response->results == NULL) {
            successful = 0;
        } else {
            response->total_matches = (int)table.size;
            result_index = 0;
            for (slot_index = 0; slot_index < table.capacity; slot_index++) {
                DocAccumulator *slot = &table.slots[slot_index];
                SearchResult *result;

                if (!slot->occupied) {
                    continue;
                }
                result = &response->results[result_index++];
                result->doc_id = slot->doc_id;
                copy_filename(
                    result->filename,
                    adapter_get_doc_filename(
                        doc_collection, slot->doc_id));
                result->score = slot->score;
                result->matched_term_count = slot->matched_term_count;
                result->matched_terms = slot->matched_terms;

                /* Transfer ownership from accumulator to response. */
                slot->matched_terms = NULL;
                slot->matched_term_count = 0;
                slot->matched_term_capacity = 0;
            }
        }
    }

    free_result_table(&table);
    free_query_terms(terms, term_count);

    if (!successful) {
        free_search_response(response);
        response = create_empty_response();
        if (response == NULL) {
            return NULL;
        }
    } else {
        if (response->total_matches > 0 &&
            sort_search_results(response) != 0) {
            free_search_response(response);
            response = create_empty_response();
            if (response == NULL) {
                return NULL;
            }
        } else if (response->total_matches > 0 &&
                   truncate_search_response(response, top_k) != 0) {
            free_search_response(response);
            response = create_empty_response();
            if (response == NULL) {
                return NULL;
            }
        }
    }

    response->query_latency_ms =
        ((double)(clock() - start_clock) * 1000.0) /
        (double)CLOCKS_PER_SEC;
    return response;
}

/*
 * ---------------------- Linear scan baseline ----------------------
 */

static int query_term_index(const QueryTerm *terms,
                            int term_count,
                            const char *token)
{
    int i;

    if (terms == NULL || token == NULL) {
        return -1;
    }
    for (i = 0; i < term_count; i++) {
        if (strcmp(terms[i].text, token) == 0) {
            return i;
        }
    }
    return -1;
}

static int append_linear_result(SearchResponse *response,
                                int *capacity,
                                const LinearScanDocument *document,
                                double score,
                                int *frequencies,
                                int term_count,
                                const QueryTerm *terms)
{
    SearchResult *grown;
    SearchResult *result;
    int matched_count;
    int i;
    int matched_index;

    if (response == NULL || capacity == NULL || document == NULL ||
        frequencies == NULL || terms == NULL) {
        return -1;
    }
    if (response->total_matches == *capacity) {
        int new_capacity = (*capacity == 0) ? 8 : *capacity * 2;
        if (new_capacity <= *capacity) {
            return -1;
        }
        grown = (SearchResult *)realloc(
            response->results,
            (size_t)new_capacity * sizeof(*response->results));
        if (grown == NULL) {
            return -1;
        }
        memset(
            grown + *capacity,
            0,
            (size_t)(new_capacity - *capacity) * sizeof(*grown));
        response->results = grown;
        *capacity = new_capacity;
    }

    result = &response->results[response->total_matches];
    memset(result, 0, sizeof(*result));
    result->doc_id = document->doc_id;
    copy_filename(result->filename, document->filename);
    result->score = score;

    matched_count = 0;
    for (i = 0; i < term_count; i++) {
        if (frequencies[i] > 0) {
            matched_count++;
        }
    }
    if (matched_count > 0) {
        result->matched_terms = (char **)calloc(
            (size_t)matched_count, sizeof(*result->matched_terms));
        if (result->matched_terms == NULL) {
            memset(result, 0, sizeof(*result));
            return -1;
        }
        matched_index = 0;
        for (i = 0; i < term_count; i++) {
            if (frequencies[i] > 0) {
                result->matched_terms[matched_index] =
                    copy_string(terms[i].text);
                if (result->matched_terms[matched_index] == NULL) {
                    int j;
                    for (j = 0; j < matched_index; j++) {
                        free(result->matched_terms[j]);
                    }
                    free(result->matched_terms);
                    result->matched_terms = NULL;
                    memset(result, 0, sizeof(*result));
                    return -1;
                }
                matched_index++;
            }
        }
    }
    result->matched_term_count = matched_count;
    response->total_matches++;
    return 0;
}

SearchResponse *search_linear_scan(const LinearScanDocument *documents,
                                   int document_count,
                                   const char *raw_query,
                                   int top_k,
                                   ScoringMethod scoring_method)
{
    SearchResponse *response;
    QueryTerm *terms;
    int term_count;
    clock_t start_clock;
    int i;
    int successful;
    int result_capacity;

    response = create_empty_response();
    if (response == NULL) {
        return NULL;
    }

    start_clock = clock();
    terms = NULL;
    term_count = 0;
    successful = 1;
    result_capacity = 0;

    if (documents == NULL || document_count <= 0 || raw_query == NULL ||
        top_k < 0 ||
        (scoring_method != SCORE_TERM_FREQUENCY &&
         scoring_method != SCORE_TF_IDF)) {
        successful = 0;
    }
    if (successful &&
        prepare_query(raw_query, &terms, &term_count) != 0) {
        successful = 0;
    }

    /*
     * Pass 1: compute DF for each query term over original text.
     */
    if (successful && terms != NULL && term_count > 0) {
        for (i = 0; i < document_count; i++) {
            char **tokens;
            int token_count;
            unsigned char *seen;
            int j;

            if (documents[i].text == NULL ||
                documents[i].total_terms < 0) {
                continue;
            }
            tokens = NULL;
            token_count = 0;
            if (adapter_tokenize_query(
                    documents[i].text, &tokens, &token_count) < 0) {
                successful = 0;
                break;
            }
            seen = (unsigned char *)calloc(
                (size_t)term_count, sizeof(*seen));
            if (seen == NULL) {
                free_token_array(tokens, token_count);
                successful = 0;
                break;
            }
            for (j = 0; j < token_count; j++) {
                int matched_index = query_term_index(
                    terms, term_count, tokens[j]);
                if (matched_index >= 0) {
                    seen[matched_index] = 1U;
                }
            }
            free_token_array(tokens, token_count);
            for (j = 0; j < term_count; j++) {
                if (seen[j] != 0U) {
                    terms[j].document_frequency++;
                }
            }
            free(seen);
        }
    }

    /*
     * Pass 2: scan matched documents, count query-term frequencies, score,
     * and materialize results.  Tokenization is repeated intentionally so
     * the baseline measures a true raw-text scan rather than a cached index.
     */
    if (successful && terms != NULL && term_count > 0) {
        for (i = 0; i < document_count; i++) {
            char **tokens;
            int token_count;
            int *frequencies;
            int j;
            int has_match;
            double score;

            if (documents[i].text == NULL ||
                documents[i].total_terms < 0) {
                continue;
            }
            tokens = NULL;
            token_count = 0;
            if (adapter_tokenize_query(
                    documents[i].text, &tokens, &token_count) < 0) {
                successful = 0;
                break;
            }
            frequencies = (int *)calloc(
                (size_t)term_count, sizeof(*frequencies));
            if (frequencies == NULL) {
                free_token_array(tokens, token_count);
                successful = 0;
                break;
            }
            for (j = 0; j < token_count; j++) {
                int matched_index = query_term_index(
                    terms, term_count, tokens[j]);
                if (matched_index >= 0 &&
                    frequencies[matched_index] < 2147483647) {
                    frequencies[matched_index]++;
                }
            }
            free_token_array(tokens, token_count);

            has_match = 0;
            score = 0.0;
            for (j = 0; j < term_count; j++) {
                if (frequencies[j] <= 0) {
                    continue;
                }
                has_match = 1;
                if (scoring_method == SCORE_TF_IDF) {
                    score += ranking_tf_idf_score(
                        frequencies[j],
                        terms[j].document_frequency,
                        document_count,
                        terms[j].multiplicity);
                } else {
                    score += ranking_term_frequency_score(
                        frequencies[j], terms[j].multiplicity);
                }
            }

            if (has_match &&
                append_linear_result(
                    response,
                    &result_capacity,
                    &documents[i],
                    score,
                    frequencies,
                    term_count,
                    terms) != 0) {
                free(frequencies);
                successful = 0;
                break;
            }
            free(frequencies);
        }
    }

    free_query_terms(terms, term_count);

    if (!successful) {
        free_search_response(response);
        response = create_empty_response();
        if (response == NULL) {
            return NULL;
        }
    } else if (response->total_matches > 0) {
        if (sort_search_results(response) != 0 ||
            truncate_search_response(response, top_k) != 0) {
            free_search_response(response);
            response = create_empty_response();
            if (response == NULL) {
                return NULL;
            }
        }
    }

    response->query_latency_ms =
        ((double)(clock() - start_clock) * 1000.0) /
        (double)CLOCKS_PER_SEC;
    return response;
}

/*
 * ------------------------- Deep release -------------------------
 */

void free_search_response(SearchResponse *response)
{
    int i;

    if (response == NULL) {
        return;
    }
    if (response->results != NULL) {
        for (i = 0; i < response->total_matches; i++) {
            int j;

            if (response->results[i].matched_terms != NULL) {
                for (j = 0;
                     j < response->results[i].matched_term_count;
                     j++) {
                    free(response->results[i].matched_terms[j]);
                }
                free(response->results[i].matched_terms);
            }
        }
    }
    free(response->results);
    free(response);
}

/*
 * ---------------------- Retrieval metrics ----------------------
 */

static int is_relevant_document(int doc_id,
                                const int *relevant_doc_ids,
                                int num_relevant)
{
    int i;

    if (relevant_doc_ids == NULL) {
        return 0;
    }
    for (i = 0; i < num_relevant; i++) {
        if (relevant_doc_ids[i] == doc_id) {
            return 1;
        }
    }
    return 0;
}

double calculate_precision_at_k(const SearchResponse *response,
                                const int *relevant_doc_ids,
                                int num_relevant,
                                int k)
{
    int relevant_found;
    int i;
    int limit;

    if (response == NULL || relevant_doc_ids == NULL ||
        num_relevant <= 0 || k <= 0) {
        return 0.0;
    }
    limit = response->returned_count;
    if (limit > k) {
        limit = k;
    }
    if (limit > response->total_matches) {
        limit = response->total_matches;
    }

    relevant_found = 0;
    for (i = 0; i < limit; i++) {
        if (is_relevant_document(
                response->results[i].doc_id,
                relevant_doc_ids,
                num_relevant)) {
            relevant_found++;
        }
    }
    return (double)relevant_found / (double)k;
}

static int count_unique_relevant(const int *relevant_doc_ids,
                                 int num_relevant)
{
    int count;
    int i;

    if (relevant_doc_ids == NULL || num_relevant <= 0) {
        return 0;
    }
    count = 0;
    for (i = 0; i < num_relevant; i++) {
        int j;
        int duplicate = 0;

        for (j = 0; j < i; j++) {
            if (relevant_doc_ids[j] == relevant_doc_ids[i]) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) {
            count++;
        }
    }
    return count;
}

double calculate_recall_at_k(const SearchResponse *response,
                             const int *relevant_doc_ids,
                             int num_relevant,
                             int k)
{
    int relevant_found;
    int unique_relevant;
    int limit;
    int i;

    if (response == NULL || relevant_doc_ids == NULL ||
        num_relevant <= 0 || k <= 0) {
        return 0.0;
    }
    unique_relevant = count_unique_relevant(
        relevant_doc_ids, num_relevant);
    if (unique_relevant == 0) {
        return 0.0;
    }

    limit = response->returned_count;
    if (limit > k) {
        limit = k;
    }
    if (limit > response->total_matches) {
        limit = response->total_matches;
    }

    relevant_found = 0;
    for (i = 0; i < limit; i++) {
        int doc_id = response->results[i].doc_id;
        int j;
        int already_counted = 0;

        if (!is_relevant_document(
                doc_id, relevant_doc_ids, num_relevant)) {
            continue;
        }
        for (j = 0; j < i; j++) {
            if (response->results[j].doc_id == doc_id) {
                already_counted = 1;
                break;
            }
        }
        if (!already_counted) {
            relevant_found++;
        }
    }
    return (double)relevant_found / (double)unique_relevant;
}

double calculate_reciprocal_rank(const SearchResponse *response,
                                 const int *relevant_doc_ids,
                                 int num_relevant)
{
    int i;
    int limit;

    if (response == NULL || relevant_doc_ids == NULL ||
        num_relevant <= 0 || response->returned_count <= 0) {
        return 0.0;
    }
    limit = response->returned_count;
    if (limit > response->total_matches) {
        limit = response->total_matches;
    }
    for (i = 0; i < limit; i++) {
        if (is_relevant_document(
                response->results[i].doc_id,
                relevant_doc_ids,
                num_relevant)) {
            return 1.0 / (double)(i + 1);
        }
    }
    return 0.0;
}
