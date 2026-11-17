#include "search.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Standalone test harness.
 *
 * The tests use a small mock collection and a mock tokenizer.  The default
 * local inverted-index definition from search.h is sufficient for the mock
 * postings, while the adapter hooks inject document metadata and tokenization.
 */

typedef struct MockDocument {
    const char *filename;
    const char *text;
    int total_terms;
} MockDocument;

static MockDocument g_documents[] = {
    {
        "doc01.txt",
        "Renewable energy can reduce dependence on fossil fuels. "
        "Solar power is widely used.",
        13
    },
    {
        "doc02.txt",
        "Wind energy and solar energy are important renewable energy sources.",
        9
    },
    {
        "doc03.txt",
        "Public transport can reduce traffic congestion in cities.",
        7
    },
    {
        "doc04.txt",
        "Geothermal energy is renewable energy from the earth.",
        8
    },
    {
        "doc05.txt",
        "Solar energy powers homes.",
        4
    },
    {
        "doc06.txt",
        "Wind energy supports wind farms.",
        4
    }
};

static int g_tests_run;
static int g_tests_failed;

static int ascii_token_character(unsigned char c)
{
    return (c >= (unsigned char)'a' && c <= (unsigned char)'z') ||
           (c >= (unsigned char)'A' && c <= (unsigned char)'Z') ||
           (c >= (unsigned char)'0' && c <= (unsigned char)'9');
}

static char ascii_lower(unsigned char c)
{
    if (c >= (unsigned char)'A' && c <= (unsigned char)'Z') {
        return (char)(c - (unsigned char)'A' + (unsigned char)'a');
    }
    return (char)c;
}

/*
 * Mock tokenizer with the same ownership contract as the production adapter:
 * each token and the token array are allocated and later freed by search.c.
 */
static int mock_tokenize_query(const char *raw_query,
                               char ***out_tokens,
                               int *out_count)
{
    char **tokens;
    int count;
    int capacity;
    size_t i;
    size_t length;

    if (out_tokens == NULL || out_count == NULL) {
        return -1;
    }
    *out_tokens = NULL;
    *out_count = 0;
    if (raw_query == NULL) {
        return 0;
    }

    tokens = NULL;
    count = 0;
    capacity = 0;
    length = strlen(raw_query);
    i = 0U;
    while (i < length) {
        size_t start;
        size_t token_length;
        size_t j;
        char *token;
        char **grown;

        while (i < length &&
               !ascii_token_character((unsigned char)raw_query[i])) {
            i++;
        }
        if (i == length) {
            break;
        }
        start = i;
        while (i < length &&
               ascii_token_character((unsigned char)raw_query[i])) {
            i++;
        }
        token_length = i - start;
        token = (char *)malloc(token_length + 1U);
        if (token == NULL) {
            int k;
            for (k = 0; k < count; k++) {
                free(tokens[k]);
            }
            free(tokens);
            return -1;
        }
        for (j = 0U; j < token_length; j++) {
            token[j] = ascii_lower(
                (unsigned char)raw_query[start + j]);
        }
        token[token_length] = '\0';

        if (count == capacity) {
            int new_capacity = (capacity == 0) ? 4 : capacity * 2;
            grown = (char **)realloc(
                tokens, (size_t)new_capacity * sizeof(*tokens));
            if (grown == NULL) {
                free(token);
                {
                    int k;
                    for (k = 0; k < count; k++) {
                        free(tokens[k]);
                    }
                }
                free(tokens);
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

static int valid_doc_index(int doc_id)
{
    return doc_id >= 1 &&
           doc_id <= (int)(sizeof(g_documents) / sizeof(g_documents[0]));
}

static const char *mock_get_doc_filename(const void *doc_collection,
                                         int doc_id)
{
    const MockDocument *documents;

    if (doc_collection == NULL || !valid_doc_index(doc_id)) {
        return NULL;
    }
    documents = (const MockDocument *)doc_collection;
    return documents[doc_id - 1].filename;
}

static int mock_get_doc_total_terms(const void *doc_collection,
                                    int doc_id)
{
    const MockDocument *documents;

    if (doc_collection == NULL || !valid_doc_index(doc_id)) {
        return -1;
    }
    documents = (const MockDocument *)doc_collection;
    return documents[doc_id - 1].total_terms;
}

static PostingNode *create_posting(int doc_id, int frequency)
{
    PostingNode *posting = (PostingNode *)malloc(sizeof(*posting));

    if (posting != NULL) {
        posting->doc_id = doc_id;
        posting->term_frequency = frequency;
        posting->next = NULL;
    }
    return posting;
}

static IndexEntry *create_index_entry(const char *term)
{
    IndexEntry *entry = (IndexEntry *)malloc(sizeof(*entry));

    if (entry == NULL) {
        return NULL;
    }
    memset(entry, 0, sizeof(*entry));
    entry->term = (char *)malloc(strlen(term) + 1U);
    if (entry->term == NULL) {
        free(entry);
        return NULL;
    }
    strcpy(entry->term, term);
    return entry;
}

static IndexEntry *find_or_add_entry(InvertedIndex *index,
                                     const char *term)
{
    IndexEntry *entry;
    IndexEntry *new_entry;

    for (entry = index->head; entry != NULL; entry = entry->next) {
        if (strcmp(entry->term, term) == 0) {
            return entry;
        }
    }

    new_entry = create_index_entry(term);
    if (new_entry == NULL) {
        return NULL;
    }
    if (index->head == NULL) {
        index->head = new_entry;
    } else {
        entry = index->head;
        while (entry->next != NULL) {
            entry = entry->next;
        }
        entry->next = new_entry;
    }
    index->unique_term_count++;
    return new_entry;
}

static int add_mock_posting(InvertedIndex *index,
                            const char *term,
                            int doc_id,
                            int frequency)
{
    IndexEntry *entry;
    PostingNode *posting;
    PostingNode *new_posting;

    entry = find_or_add_entry(index, term);
    if (entry == NULL) {
        return -1;
    }

    posting = entry->postings;
    while (posting != NULL) {
        if (posting->doc_id == doc_id) {
            posting->term_frequency += frequency;
            return 0;
        }
        posting = posting->next;
    }

    new_posting = create_posting(doc_id, frequency);
    if (new_posting == NULL) {
        return -1;
    }
    new_posting->next = entry->postings;
    entry->postings = new_posting;
    return 0;
}

static void free_mock_index(InvertedIndex *index)
{
    IndexEntry *entry;

    if (index == NULL) {
        return;
    }
    entry = index->head;
    while (entry != NULL) {
        IndexEntry *next_entry = entry->next;
        PostingNode *posting = entry->postings;

        while (posting != NULL) {
            PostingNode *next_posting = posting->next;
            free(posting);
            posting = next_posting;
        }
        free(entry->term);
        free(entry);
        entry = next_entry;
    }
    memset(index, 0, sizeof(*index));
}

static int build_mock_index(InvertedIndex *index)
{
    memset(index, 0, sizeof(*index));
    index->document_count =
        (int)(sizeof(g_documents) / sizeof(g_documents[0]));

    if (add_mock_posting(index, "renewable", 1, 1) != 0 ||
        add_mock_posting(index, "renewable", 2, 1) != 0 ||
        add_mock_posting(index, "renewable", 4, 1) != 0 ||
        add_mock_posting(index, "energy", 1, 1) != 0 ||
        add_mock_posting(index, "energy", 2, 3) != 0 ||
        add_mock_posting(index, "energy", 4, 2) != 0 ||
        add_mock_posting(index, "energy", 5, 1) != 0 ||
        add_mock_posting(index, "energy", 6, 1) != 0 ||
        add_mock_posting(index, "geothermal", 4, 1) != 0 ||
        add_mock_posting(index, "solar", 1, 1) != 0 ||
        add_mock_posting(index, "solar", 2, 1) != 0 ||
        add_mock_posting(index, "solar", 5, 1) != 0 ||
        add_mock_posting(index, "wind", 2, 1) != 0 ||
        add_mock_posting(index, "wind", 6, 2) != 0) {
        free_mock_index(index);
        return -1;
    }
    return 0;
}

static int expect_true(int condition,
                       const char *test_name,
                       const char *message)
{
    if (!condition) {
        printf("  FAIL: %s - %s\n", test_name, message);
        return 0;
    }
    return 1;
}

static int expect_ids(const SearchResponse *response,
                      const int *expected_ids,
                      int expected_count,
                      const char *test_name)
{
    int i;
    int passed;

    passed = expect_true(
        response != NULL,
        test_name,
        "response is NULL");
    if (!passed) {
        return 0;
    }
    passed = expect_true(
        response->returned_count == expected_count,
        test_name,
        "returned_count does not match");
    if (!passed) {
        return 0;
    }
    for (i = 0; i < expected_count; i++) {
        if (!expect_true(
                response->results[i].doc_id == expected_ids[i],
                test_name,
                "doc_id order does not match")) {
            return 0;
        }
    }
    return 1;
}

static void finish_test(const char *name, int passed)
{
    g_tests_run++;
    if (!passed) {
        g_tests_failed++;
        return;
    }
    printf("PASS: %s\n", name);
}

static void test_single_term(const InvertedIndex *index)
{
    const int expected[] = {1, 2, 4};
    SearchResponse *response = search_indexed(
        index, g_documents, 6, "renewable", 10, SCORE_TERM_FREQUENCY);
    SearchResponse *tfidf = search_indexed(
        index, g_documents, 6, "renewable", 10, SCORE_TF_IDF);
    double expected_idf = log10(6.0 / 3.0);
    int passed = expect_ids(
        response, expected, 3, "single-term renewable");

    if (response != NULL) {
        passed = expect_true(
            fabs(response->results[2].score - 1.0) < 1e-9,
            "single-term renewable",
            "TF score should be 1.0") && passed;
    }
    passed = expect_ids(
        tfidf, expected, 3, "single-term TF-IDF") && passed;
    if (tfidf != NULL) {
        passed = expect_true(
            isfinite(tfidf->results[0].score) &&
                fabs(tfidf->results[0].score - expected_idf) < 1e-9,
            "single-term TF-IDF",
            "TF-IDF formula should use TF * log10(N / DF)") && passed;
    }
    free_search_response(response);
    free_search_response(tfidf);
    finish_test("1. Single-term search", passed);
}

static void test_common_term(const InvertedIndex *index)
{
    const int expected[] = {2, 4, 1, 5, 6};
    const int expected_top_two[] = {2, 4};
    SearchResponse *response = search_indexed(
        index, g_documents, 6, "energy", 10, SCORE_TERM_FREQUENCY);
    SearchResponse *top_two = search_indexed(
        index, g_documents, 6, "energy", 2, SCORE_TERM_FREQUENCY);
    int passed = expect_ids(
        response, expected, 5, "common-term energy");

    if (response != NULL) {
        passed = expect_true(
            fabs(response->results[0].score - 3.0) < 1e-9,
            "common-term energy",
            "doc02 TF score should be 3.0") && passed;
    }
    passed = expect_ids(
        top_two, expected_top_two, 2, "Top-K truncation") && passed;
    if (top_two != NULL) {
        passed = expect_true(
            top_two->total_matches == 5 &&
                top_two->returned_count == 2,
            "Top-K truncation",
            "total_matches must be retained after truncation") && passed;
    }
    free_search_response(response);
    free_search_response(top_two);
    finish_test("2. Common-term search", passed);
}

static void test_rare_term(const InvertedIndex *index)
{
    const int expected[] = {4};
    SearchResponse *response = search_indexed(
        index, g_documents, 6, "geothermal", 10, SCORE_TERM_FREQUENCY);
    int passed = expect_ids(
        response, expected, 1, "rare-term geothermal");

    free_search_response(response);
    finish_test("3. Rare-term search", passed);
}

static LinearScanDocument g_linear_documents[] = {
    {1, "doc01.txt",
     "Renewable energy can reduce dependence on fossil fuels. "
     "Solar power is widely used.", 13},
    {2, "doc02.txt",
     "Wind energy and solar energy are important renewable energy sources.",
     9},
    {3, "doc03.txt",
     "Public transport can reduce traffic congestion in cities.", 7},
    {4, "doc04.txt",
     "Geothermal energy is renewable energy from the earth.", 8},
    {5, "doc05.txt", "Solar energy powers homes.", 4},
    {6, "doc06.txt", "Wind energy supports wind farms.", 4}
};

static void test_multiple_terms(const InvertedIndex *index)
{
    /*
     * OR semantics: documents 1, 2 and 4 match both terms, while documents
     * 5 and 6 match "energy" only.
     */
    const int expected[] = {2, 4, 1, 5, 6};
    SearchResponse *response = search_indexed(
        index, g_documents, 6, "renewable energy", 10,
        SCORE_TERM_FREQUENCY);
    SearchResponse *linear = search_linear_scan(
        g_linear_documents, 6, "renewable energy", 10,
        SCORE_TERM_FREQUENCY);
    int passed = expect_ids(
        response, expected, 5, "multiple-term renewable energy");

    passed = expect_ids(
        linear, expected, 5, "linear baseline") && passed;
    if (response != NULL) {
        passed = expect_true(
            response->results[0].matched_term_count == 2,
            "multiple-term renewable energy",
            "doc02 should match two unique terms") && passed;
    }
    if (response != NULL && linear != NULL) {
        printf("  Indexed latency: %.6f ms; linear latency: %.6f ms\n",
               response->query_latency_ms,
               linear->query_latency_ms);
    }
    if (response != NULL) {
        printf("  Sample indexed result table:\n");
        print_search_results(response, 3);
    }
    free_search_response(response);
    free_search_response(linear);
    finish_test("4. Multiple-term search", passed);
}

static void test_no_match(const InvertedIndex *index)
{
    SearchResponse *response = search_indexed(
        index, g_documents, 6, "quantumbanana", 10,
        SCORE_TERM_FREQUENCY);
    int passed = expect_true(
        response != NULL &&
            response->total_matches == 0 &&
            response->returned_count == 0,
        "no-match query",
        "response should be empty");

    free_search_response(response);
    finish_test("5. No-match search", passed);
}

static void test_punctuation(const InvertedIndex *index)
{
    const int expected[] = {2, 4, 1, 5, 6};
    SearchResponse *response = search_indexed(
        index, g_documents, 6, "renewable-energy", 10,
        SCORE_TERM_FREQUENCY);
    int passed = expect_ids(
        response, expected, 5, "punctuation normalization");

    free_search_response(response);
    finish_test("6. Punctuation query", passed);
}

static void test_repeated_term(const InvertedIndex *index)
{
    const int expected[] = {2, 4, 1, 5, 6};
    SearchResponse *response = search_indexed(
        index, g_documents, 6, "energy energy", 10,
        SCORE_TERM_FREQUENCY);
    int passed = expect_ids(
        response, expected, 5, "repeated term");

    if (response != NULL) {
        passed = expect_true(
            fabs(response->results[0].score - 6.0) < 1e-9,
            "repeated term",
            "repeated query should double the TF contribution") && passed;
        passed = expect_true(
            response->results[0].matched_term_count == 1,
            "repeated term",
            "matched_terms should be deduplicated") && passed;
    }
    free_search_response(response);
    finish_test("7. Repeated-term search", passed);
}

static void test_invalid_inputs(const InvertedIndex *index)
{
    SearchResponse *empty_query;
    SearchResponse *spaces;
    SearchResponse *punctuation;
    SearchResponse *null_query;
    SearchResponse *null_index;
    int passed;

    empty_query = search_indexed(
        index, g_documents, 6, "", 10, SCORE_TERM_FREQUENCY);
    spaces = search_indexed(
        index, g_documents, 6, "   ", 10, SCORE_TERM_FREQUENCY);
    punctuation = search_indexed(
        index, g_documents, 6, "   ---  ", 10, SCORE_TERM_FREQUENCY);
    null_query = search_indexed(
        index, g_documents, 6, NULL, 10, SCORE_TERM_FREQUENCY);
    null_index = search_indexed(
        NULL, g_documents, 6, "energy", 10, SCORE_TERM_FREQUENCY);

    passed = expect_true(
        empty_query != NULL && empty_query->total_matches == 0,
        "invalid input", "empty string must return empty response");
    passed = expect_true(
        spaces != NULL && spaces->total_matches == 0,
        "invalid input", "spaces must return empty response") && passed;
    passed = expect_true(
        punctuation != NULL && punctuation->total_matches == 0,
        "invalid input", "punctuation must return empty response") && passed;
    passed = expect_true(
        null_query != NULL && null_query->total_matches == 0,
        "invalid input", "NULL query must return empty response") && passed;
    passed = expect_true(
        null_index != NULL && null_index->total_matches == 0,
        "invalid input", "NULL index must return empty response") && passed;

    free_search_response(empty_query);
    free_search_response(spaces);
    free_search_response(punctuation);
    free_search_response(null_query);
    free_search_response(null_index);
    finish_test("8. Empty and invalid input", passed);
}

static void test_tie_breaking(const InvertedIndex *index)
{
    const int expected[] = {2, 4, 1, 5, 6};
    SearchResponse *response = search_indexed(
        index, g_documents, 6, "energy", 10, SCORE_TERM_FREQUENCY);
    int passed = expect_ids(
        response, expected, 5, "tie-breaking");

    if (response != NULL && response->returned_count >= 5) {
        passed = expect_true(
            fabs(response->results[2].score -
                 response->results[3].score) < 1e-9,
            "tie-breaking", "docs should have equal score") && passed;
        passed = expect_true(
            response->results[2].doc_id <
                response->results[3].doc_id &&
            response->results[3].doc_id <
                response->results[4].doc_id,
            "tie-breaking", "equal scores must sort by doc_id ascending") &&
            passed;
    }
    free_search_response(response);
    finish_test("9. Tie-breaking", passed);
}

static void test_quality_metrics(const InvertedIndex *index)
{
    const int relevant[] = {4, 999};
    const int relevant_with_duplicate[] = {4, 4, 999};
    SearchResponse *response = search_indexed(
        index, g_documents, 6, "energy", 5, SCORE_TERM_FREQUENCY);
    double precision;
    double recall;
    double reciprocal_rank;
    double duplicate_recall;
    int passed;

    precision = calculate_precision_at_k(
        response, relevant, 2, 2);
    recall = calculate_recall_at_k(
        response, relevant, 2, 2);
    reciprocal_rank = calculate_reciprocal_rank(
        response, relevant, 2);
    duplicate_recall = calculate_recall_at_k(
        response, relevant_with_duplicate, 3, 2);

    passed = expect_true(
        fabs(precision - 0.5) < 1e-9,
        "quality metrics", "Precision@2 should be 0.5");
    passed = expect_true(
        fabs(recall - 0.5) < 1e-9,
        "quality metrics", "Recall@2 should be 0.5") && passed;
    passed = expect_true(
        fabs(reciprocal_rank - 0.5) < 1e-9,
        "quality metrics", "first relevant doc should have RR 0.5") &&
        passed;
    passed = expect_true(
        fabs(duplicate_recall - 0.5) < 1e-9,
        "quality metrics",
        "duplicate qrel IDs must not change Recall denominator") &&
        passed;

    free_search_response(response);
    finish_test("10. Precision, Recall and MRR inputs", passed);
}

int main(void)
{
    SearchAdapterHooks hooks;
    InvertedIndex index;

    memset(&hooks, 0, sizeof(hooks));
    hooks.tokenize_query = mock_tokenize_query;
    hooks.lookup_term = NULL; /* Use the local mock inverted-index lookup. */
    hooks.get_doc_filename = mock_get_doc_filename;
    hooks.get_doc_total_terms = mock_get_doc_total_terms;
    search_set_adapter_hooks(&hooks);

    if (build_mock_index(&index) != 0) {
        fprintf(stderr, "Failed to build the mock inverted index.\n");
        search_clear_adapter_hooks();
        return EXIT_FAILURE;
    }

    printf("COMP1028 search/ranking unit tests\n");
    printf("==================================\n");
    test_single_term(&index);
    test_common_term(&index);
    test_rare_term(&index);
    test_multiple_terms(&index);
    test_no_match(&index);
    test_punctuation(&index);
    test_repeated_term(&index);
    test_invalid_inputs(&index);
    test_tie_breaking(&index);
    test_quality_metrics(&index);

    free_mock_index(&index);
    search_clear_adapter_hooks();

    printf("==================================\n");
    printf("Tests run: %d, failed: %d\n", g_tests_run, g_tests_failed);
    if (g_tests_failed != 0) {
        return EXIT_FAILURE;
    }
    printf("All tests passed.\n");
    return EXIT_SUCCESS;
}
