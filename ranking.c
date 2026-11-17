#include "search.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Base course formula:
 *
 *   score(doc, query) = sum of TF(term, doc)
 *
 * If the user repeats a query term, its multiplicity is retained and the
 * corresponding term contribution is weighted accordingly.  matched_terms
 * is still deduplicated for display by search.c.
 */
double ranking_term_frequency_score(int term_frequency,
                                    int query_multiplicity)
{
    if (term_frequency <= 0 || query_multiplicity <= 0) {
        return 0.0;
    }
    return (double)term_frequency * (double)query_multiplicity;
}

/*
 * Deterministic TF-IDF formula required for this module:
 *
 *   contribution = TF * log10(N / DF) * query multiplicity
 *
 * N is the number of documents in the collection and DF is the number of
 * postings for that term.  For malformed metadata, the function returns 0
 * rather than dividing by zero or returning NaN.  IDF is clamped at zero if
 * DF > N, which should not happen in a valid index but is handled defensively.
 */
double ranking_tf_idf_score(int term_frequency,
                            int document_frequency,
                            int collection_document_count,
                            int query_multiplicity)
{
    double idf;

    if (term_frequency <= 0 || document_frequency <= 0 ||
        collection_document_count <= 0 || query_multiplicity <= 0) {
        return 0.0;
    }

    idf = log10(
        (double)collection_document_count / (double)document_frequency);
    if (idf < 0.0 || !isfinite(idf)) {
        idf = 0.0;
    }
    return (double)term_frequency * idf * (double)query_multiplicity;
}

/*
 * Return negative when left should appear after right.
 * Return positive when left should appear before right.
 * Exact scores use doc_id ascending for deterministic tie-breaking.
 */
static int result_precedes(const SearchResult *left,
                           const SearchResult *right)
{
    if (fabs(left->score - right->score) < 1e-9) {
        if (left->doc_id < right->doc_id) {
            return 1;
        }
        if (left->doc_id > right->doc_id) {
            return -1;
        }
        return 0;
    }
    return (left->score > right->score) ? 1 : -1;
}

/*
 * Stable recursive merge sort.  The temporary array is allocated once by
 * sort_search_results(), so recursive calls do not allocate or leak memory.
 */
static void merge_sort_range(SearchResult *items,
                             SearchResult *temporary,
                             size_t begin,
                             size_t end)
{
    size_t middle;
    size_t left;
    size_t right;
    size_t output;

    if (end - begin < 2U) {
        return;
    }

    middle = begin + (end - begin) / 2U;
    merge_sort_range(items, temporary, begin, middle);
    merge_sort_range(items, temporary, middle, end);

    left = begin;
    right = middle;
    output = begin;
    while (left < middle && right < end) {
        if (result_precedes(&items[right], &items[left]) > 0) {
            temporary[output++] = items[right++];
        } else {
            temporary[output++] = items[left++];
        }
    }
    while (left < middle) {
        temporary[output++] = items[left++];
    }
    while (right < end) {
        temporary[output++] = items[right++];
    }

    memcpy(items + begin,
           temporary + begin,
           (end - begin) * sizeof(*items));
}

/*
 * Allocation-free fallback for an extremely low-memory situation.  The
 * primary path remains merge sort; this fallback avoids returning an
 * unsorted response if the temporary merge buffer cannot be allocated.
 */
static void insertion_sort_fallback(SearchResult *items, size_t count)
{
    size_t i;

    for (i = 1U; i < count; i++) {
        SearchResult value = items[i];
        size_t position = i;

        while (position > 0U &&
               result_precedes(&value, &items[position - 1U]) > 0) {
            items[position] = items[position - 1U];
            position--;
        }
        items[position] = value;
    }
}

int sort_search_results(SearchResponse *response)
{
    SearchResult *temporary;
    size_t count;

    if (response == NULL || response->total_matches < 0) {
        return -1;
    }
    if (response->total_matches < 2 || response->results == NULL) {
        return 0;
    }

    count = (size_t)response->total_matches;
    if (count > ((size_t)-1) / sizeof(*temporary)) {
        return -1;
    }
    temporary = (SearchResult *)malloc(count * sizeof(*temporary));
    if (temporary == NULL) {
        insertion_sort_fallback(response->results, count);
        return 0;
    }

    merge_sort_range(response->results, temporary, 0U, count);
    free(temporary);
    return 0;
}

static void release_result_fields(SearchResult *result)
{
    int i;

    if (result == NULL) {
        return;
    }
    if (result->matched_terms != NULL) {
        for (i = 0; i < result->matched_term_count; i++) {
            free(result->matched_terms[i]);
        }
        free(result->matched_terms);
    }
    memset(result, 0, sizeof(*result));
}

int truncate_search_response(SearchResponse *response, int top_k)
{
    int final_count;
    int i;

    if (response == NULL || top_k < 0) {
        return -1;
    }
    if (top_k < response->total_matches) {
        final_count = top_k;
    } else {
        final_count = response->total_matches;
    }

    for (i = final_count; i < response->total_matches; i++) {
        release_result_fields(&response->results[i]);
    }
    response->returned_count = final_count;
    return 0;
}

static void print_matched_terms(const SearchResult *result)
{
    int i;

    if (result == NULL || result->matched_term_count <= 0 ||
        result->matched_terms == NULL) {
        printf("-");
        return;
    }
    for (i = 0; i < result->matched_term_count; i++) {
        if (i > 0) {
            printf(", ");
        }
        printf("%s", result->matched_terms[i]);
    }
}

void print_search_results(const SearchResponse *response, int top_k)
{
    int count;
    int i;

    if (response == NULL || response->total_matches <= 0 ||
        response->returned_count <= 0) {
        printf("No matching documents found.\n");
        return;
    }

    count = response->returned_count;
    if (top_k >= 0 && top_k < count) {
        count = top_k;
    }

    printf("%-6s | %-30s | %10s | %s\n",
           "Rank", "File", "Score", "Matched Terms");
    printf("-------+--------------------------------"
           "+------------+--------------------------\n");
    for (i = 0; i < count; i++) {
        const SearchResult *result = &response->results[i];
        printf("%-6d | %-30.30s | %10.6f | ",
               i + 1, result->filename, result->score);
        print_matched_terms(result);
        printf("\n");
    }
}
