#include "index.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BIG_DOCS 50
#define BIG_WORDS 2000
#define BIG_VOCAB 300

static const Document g_sample[2] = {
    {0, "tmp_sample0.txt"},
    {1, "tmp_sample1.txt"}
};

static int g_tests;
static int g_failed;

int next_word(FILE *file, char *word, int size)
{
    int c;
    int n = 0;

    if (file == NULL || word == NULL || size <= 0) {
        return -1;
    }
    c = fgetc(file);
    while (c != EOF && !isalnum((unsigned char)c)) {
        c = fgetc(file);
    }
    if (c == EOF) {
        return ferror(file) ? -1 : 0;
    }
    while (c != EOF && isalnum((unsigned char)c)) {
        if (n < size - 1) {
            word[n++] = (char)tolower((unsigned char)c);
        }
        c = fgetc(file);
    }
    word[n] = '\0';
    return 1;
}

static int check(int ok, const char *name)
{
    g_tests++;
    if (!ok) {
        g_failed++;
    }
    printf("%-48s %s\n", name, ok ? "PASS" : "FAIL");
    return ok;
}

static const IndexEntry *find_entry(const InvertedIndex *index,
                                    const char *word)
{
    const IndexEntry *entry;

    for (entry = index->head; entry != NULL; entry = entry->next) {
        if (strcmp(entry->term, word) == 0) {
            return entry;
        }
    }
    return NULL;
}

static const PostingNode *find_posting(const IndexEntry *entry, int doc_id)
{
    const PostingNode *posting;

    if (entry == NULL) {
        return NULL;
    }
    for (posting = entry->postings; posting != NULL; posting = posting->next) {
        if (posting->doc_id == doc_id) {
            return posting;
        }
    }
    return NULL;
}

static double score_query(const InvertedIndex *index,
                          const char *const *terms,
                          int term_count,
                          int doc_id)
{
    double score = 0.0;
    int i;

    for (i = 0; i < term_count; i++) {
        const PostingNode *posting = index_lookup(index, terms[i]);

        while (posting != NULL) {
            if (posting->doc_id == doc_id) {
                score += (double)posting->term_frequency;
                break;
            }
            posting = posting->next;
        }
    }
    return score;
}

static int write_text_file(const char *name, const char *text)
{
    FILE *file = fopen(name, "w");

    if (file == NULL) {
        return -1;
    }
    if (fputs(text, file) == EOF) {
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static int write_sample_files(void)
{
    if (write_text_file(g_sample[0].filename,
                        "solar energy energy") != 0) {
        return -1;
    }
    if (write_text_file(g_sample[1].filename, "wind energy") != 0) {
        return -1;
    }
    return 0;
}

static void remove_sample_files(void)
{
    remove(g_sample[0].filename);
    remove(g_sample[1].filename);
}

static int build_sample(InvertedIndex *index)
{
    if (write_sample_files() != 0) {
        return -1;
    }
    return index_build_from_documents(index, g_sample, 2);
}

static int total_frequency(const InvertedIndex *index)
{
    const IndexEntry *entry;
    int total = 0;

    for (entry = index->head; entry != NULL; entry = entry->next) {
        const PostingNode *posting;

        for (posting = entry->postings; posting != NULL;
             posting = posting->next) {
            total += posting->term_frequency;
        }
    }
    return total;
}

static void test_same_document(void)
{
    InvertedIndex index;
    const IndexEntry *entry;
    const PostingNode *posting;
    int ok;

    memset(&index, 0, sizeof(index));
    ok = index_add_occurrence(&index, "energy", 0) == 0;
    ok = ok && index_add_occurrence(&index, "energy", 0) == 0;
    entry = find_entry(&index, "energy");
    posting = find_posting(entry, 0);
    ok = ok && entry != NULL && posting != NULL &&
         posting->term_frequency == 2 &&
         posting->next == NULL &&
         index.unique_term_count == 1 &&
         index_posting_count(&index) == 1;
    check(ok, "1. same word twice in one doc");
    index_free(&index);
}

static void test_two_documents(void)
{
    InvertedIndex index;
    const IndexEntry *entry;
    const PostingNode *first;
    const PostingNode *second;
    int ok;

    memset(&index, 0, sizeof(index));
    ok = index_add_occurrence(&index, "solar", 0) == 0;
    ok = ok && index_add_occurrence(&index, "solar", 1) == 0;
    entry = find_entry(&index, "solar");
    first = find_posting(entry, 0);
    second = find_posting(entry, 1);
    ok = ok && entry != NULL && first != NULL && second != NULL &&
         first->term_frequency == 1 && second->term_frequency == 1 &&
         index.unique_term_count == 1 &&
         index_posting_count(&index) == 2;
    check(ok, "2. same word in two docs");
    index_free(&index);
}

static void test_new_word(void)
{
    InvertedIndex index;
    int ok;

    memset(&index, 0, sizeof(index));
    ok = index_add_occurrence(&index, "alpha", 0) == 0;
    ok = ok && index.unique_term_count == 1;
    ok = ok && index_add_occurrence(&index, "beta", 0) == 0;
    ok = ok && index.unique_term_count == 2;
    ok = ok && index_add_occurrence(&index, "alpha", 0) == 0;
    ok = ok && index.unique_term_count == 2;
    check(ok, "3. new word adds one unique term");
    index_free(&index);
}

static void test_unknown_word(void)
{
    InvertedIndex index;
    const PostingNode *postings;
    int ok;

    memset(&index, 0, sizeof(index));
    index_add_occurrence(&index, "solar", 0);
    postings = index_lookup(&index, "solar");
    ok = postings != NULL && postings->doc_id == 0 &&
         postings->term_frequency == 1;
    ok = ok && index_lookup(&index, "missing") == NULL;
    check(ok, "4. lookup finds known words, unknown is NULL");
    index_free(&index);
}

static void test_empty_file(void)
{
    const Document docs[1] = {{0, "tmp_empty.txt"}};
    InvertedIndex index;
    int ok;

    memset(&index, 0, sizeof(index));
    ok = write_text_file(docs[0].filename, "") == 0;
    ok = ok && index_build_from_documents(&index, docs, 1) == 0;
    ok = ok && index.head == NULL &&
         index.document_count == 1 &&
         index.unique_term_count == 0 &&
         index_posting_count(&index) == 0;
    check(ok, "5. empty file builds an empty index");
    index_free(&index);
    remove(docs[0].filename);
}

static void test_rebuild(void)
{
    InvertedIndex index;
    int ok;
    int energy0;
    int energy1;

    memset(&index, 0, sizeof(index));
    ok = build_sample(&index) == 0;
    energy0 = find_posting(find_entry(&index, "energy"), 0)
                  ? find_posting(find_entry(&index, "energy"), 0)->term_frequency
                  : 0;
    energy1 = find_posting(find_entry(&index, "energy"), 1)
                  ? find_posting(find_entry(&index, "energy"), 1)->term_frequency
                  : 0;
    ok = ok && index_posting_count(&index) == 4 &&
         index.unique_term_count == 3;

    ok = ok && build_sample(&index) == 0;
    ok = ok && index_posting_count(&index) == 4 &&
         index.unique_term_count == 3 &&
         index.document_count == 2;
    ok = ok && find_posting(find_entry(&index, "energy"), 0) != NULL &&
         find_posting(find_entry(&index, "energy"), 0)->term_frequency == energy0;
    ok = ok && find_posting(find_entry(&index, "energy"), 1) != NULL &&
         find_posting(find_entry(&index, "energy"), 1)->term_frequency == energy1;
    check(ok, "6. rebuild keeps the same frequencies");
    index_free(&index);
    remove_sample_files();
}

static void test_free_and_rebuild(void)
{
    InvertedIndex index;
    int ok;

    memset(&index, 0, sizeof(index));
    ok = build_sample(&index) == 0;
    index_free(&index);
    ok = ok && index.head == NULL &&
         index.document_count == 0 &&
         index.unique_term_count == 0;
    ok = ok && build_sample(&index) == 0;
    ok = ok && index.unique_term_count == 3 &&
         index_posting_count(&index) == 4;
    check(ok, "7. free then rebuild works");
    index_free(&index);
    remove_sample_files();
}

static int make_big_corpus(Document *docs, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        char name[32];
        FILE *file;
        int j;

        sprintf(name, "tmp_big_%02d.txt", i);
        docs[i].id = i;
        strcpy(docs[i].filename, name);
        file = fopen(name, "w");
        if (file == NULL) {
            return i;
        }
        for (j = 0; j < BIG_WORDS; j++) {
            fprintf(file, "w%03d ", (i * 37 + j * 11) % BIG_VOCAB);
        }
        fclose(file);
    }
    return count;
}

static void remove_documents(const Document *docs, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        remove(docs[i].filename);
    }
}

static void test_big_corpus(void)
{
    Document docs[BIG_DOCS];
    InvertedIndex index;
    clock_t start;
    clock_t finish;
    double seconds = 0.0;
    int made;
    int ok;

    made = make_big_corpus(docs, BIG_DOCS);
    memset(&index, 0, sizeof(index));
    ok = made == BIG_DOCS;
    if (ok) {
        start = clock();
        ok = index_build_from_documents(&index, docs, BIG_DOCS) == 0;
        finish = clock();
        seconds = (double)(finish - start) / (double)CLOCKS_PER_SEC;
    }
    ok = ok && index.document_count == BIG_DOCS &&
         index.unique_term_count == BIG_VOCAB &&
         index_posting_count(&index) == BIG_DOCS * BIG_VOCAB &&
         total_frequency(&index) == BIG_DOCS * BIG_WORDS;
    printf("  50 docs / 100000 words: terms=%d postings=%d time=%.3f s\n",
           index.unique_term_count, index_posting_count(&index), seconds);
    check(ok, "8. 50 docs, 100000 words");
    index_free(&index);
    remove_documents(docs, made);
}

static void test_query_scores(void)
{
    static const char *query[2] = {"solar", "energy"};
    InvertedIndex index;
    double doc0;
    double doc1;
    int ok;

    memset(&index, 0, sizeof(index));
    ok = build_sample(&index) == 0;
    doc0 = score_query(&index, query, 2, 0);
    doc1 = score_query(&index, query, 2, 1);
    ok = ok && fabs(doc0 - 3.0) < 1e-9 && fabs(doc1 - 1.0) < 1e-9;
    check(ok, "9. query solar energy gives 3 and 1");
    index_free(&index);
    remove_sample_files();
}

static void test_save_and_load(void)
{
    static const char *query[2] = {"solar", "energy"};
    InvertedIndex index;
    double before0;
    double before1;
    double after0;
    double after1;
    int ok;

    memset(&index, 0, sizeof(index));
    ok = build_sample(&index) == 0;
    before0 = score_query(&index, query, 2, 0);
    before1 = score_query(&index, query, 2, 1);
    ok = ok && index_save(&index, "tmp_index.txt") == 0;
    index_free(&index);
    ok = ok && index_load(&index, "tmp_index.txt") == 0;
    ok = ok && index.document_count == 2 &&
         index.unique_term_count == 3 &&
         index_posting_count(&index) == 4;
    after0 = score_query(&index, query, 2, 0);
    after1 = score_query(&index, query, 2, 1);
    ok = ok && fabs(before0 - after0) < 1e-9 &&
         fabs(before1 - after1) < 1e-9;
    check(ok, "10. save and load keeps query scores");
    index_free(&index);
    remove("tmp_index.txt");
    remove_sample_files();
}

static void test_missing_file(void)
{
    const Document docs[1] = {{0, "tmp_missing_file.txt"}};
    InvertedIndex index;
    int ok;

    remove(docs[0].filename);
    memset(&index, 0, sizeof(index));
    ok = index_build_from_documents(&index, docs, 1) == -1;
    ok = ok && index.head == NULL &&
         index.document_count == 0 &&
         index.unique_term_count == 0 &&
         index_posting_count(&index) == 0;
    check(ok, "11. missing file fails and clears index");
    index_free(&index);
}

int main(void)
{
    printf("COMP1028 B index tests\n");
    printf("======================\n");
    test_same_document();
    test_two_documents();
    test_new_word();
    test_unknown_word();
    test_empty_file();
    test_rebuild();
    test_free_and_rebuild();
    test_big_corpus();
    test_query_scores();
    test_save_and_load();
    test_missing_file();
    printf("======================\n");
    printf("tests: %d, failed: %d\n", g_tests, g_failed);
    return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
