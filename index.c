#include "index.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static IndexEntry *find_entry(InvertedIndex *index, const char *word)
{
    IndexEntry *entry;

    for (entry = index->head; entry != NULL; entry = entry->next) {
        if (strcmp(entry->term, word) == 0) {
            return entry;
        }
    }
    return NULL;
}

static IndexEntry *create_entry(InvertedIndex *index, const char *word)
{
    IndexEntry *entry;
    char *copy;
    size_t length;

    entry = (IndexEntry *)malloc(sizeof(*entry));
    if (entry == NULL) {
        return NULL;
    }
    length = strlen(word);
    copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        free(entry);
        return NULL;
    }
    strcpy(copy, word);
    entry->term = copy;
    entry->postings = NULL;
    entry->next = index->head;
    index->head = entry;
    index->unique_term_count++;
    return entry;
}

int index_add_occurrence(InvertedIndex *index, const char *word, int doc_id)
{
    IndexEntry *entry;
    PostingNode *posting;
    int new_entry = 0;

    if (index == NULL || word == NULL) {
        return -1;
    }
    if (word[0] == '\0') {
        return 0;
    }
    if (doc_id < 0) {
        return -1;
    }

    entry = find_entry(index, word);
    if (entry == NULL) {
        entry = create_entry(index, word);
        if (entry == NULL) {
            return -1;
        }
        new_entry = 1;
    }

    for (posting = entry->postings; posting != NULL; posting = posting->next) {
        if (posting->doc_id == doc_id) {
            posting->term_frequency++;
            return 0;
        }
    }

    posting = (PostingNode *)malloc(sizeof(*posting));
    if (posting == NULL) {
        if (new_entry) {
            index->head = entry->next;
            free(entry->term);
            free(entry);
            index->unique_term_count--;
        }
        return -1;
    }
    posting->doc_id = doc_id;
    posting->term_frequency = 1;
    posting->next = entry->postings;
    entry->postings = posting;
    return 0;
}

int index_build_from_documents(InvertedIndex *index,
                               const Document *docs,
                               int count)
{
    int i;

    if (index == NULL || docs == NULL || count <= 0) {
        return -1;
    }

    index_free(index);
    index->document_count = count;

    for (i = 0; i < count; i++) {
        FILE *file;
        char word[INDEX_WORD_LEN];
        int status;

        file = fopen(docs[i].filename, "r");
        if (file == NULL) {
            index_free(index);
            return -1;
        }
        while ((status = next_word(file, word, INDEX_WORD_LEN)) == 1) {
            if (index_add_occurrence(index, word, docs[i].id) != 0) {
                fclose(file);
                index_free(index);
                return -1;
            }
        }
        fclose(file);
        if (status < 0) {
            index_free(index);
            return -1;
        }
    }
    return 0;
}

void index_free(InvertedIndex *index)
{
    IndexEntry *entry;
    IndexEntry *next_entry;

    if (index == NULL) {
        return;
    }
    entry = index->head;
    while (entry != NULL) {
        PostingNode *posting = entry->postings;

        while (posting != NULL) {
            PostingNode *next_posting = posting->next;

            free(posting);
            posting = next_posting;
        }
        next_entry = entry->next;
        free(entry->term);
        free(entry);
        entry = next_entry;
    }
    index->head = NULL;
    index->document_count = 0;
    index->unique_term_count = 0;
}

const PostingNode *index_lookup(const InvertedIndex *index, const char *word)
{
    const IndexEntry *entry;

    if (index == NULL || word == NULL) {
        return NULL;
    }
    for (entry = index->head; entry != NULL; entry = entry->next) {
        if (strcmp(entry->term, word) == 0) {
            return entry->postings;
        }
    }
    return NULL;
}

int index_posting_count(const InvertedIndex *index)
{
    const IndexEntry *entry;
    int count = 0;

    if (index == NULL) {
        return 0;
    }
    for (entry = index->head; entry != NULL; entry = entry->next) {
        const PostingNode *posting;

        for (posting = entry->postings; posting != NULL;
             posting = posting->next) {
            count++;
        }
    }
    return count;
}

int index_save(const InvertedIndex *index, const char *path)
{
    const IndexEntry *entry;
    FILE *file;

    if (index == NULL || path == NULL) {
        return -1;
    }
    file = fopen(path, "w");
    if (file == NULL) {
        return -1;
    }
    if (fprintf(file, "COMP1028_INDEX 1\n") < 0 ||
        fprintf(file, "%d %d %d\n", index->document_count,
                index->unique_term_count, index_posting_count(index)) < 0) {
        fclose(file);
        return -1;
    }
    for (entry = index->head; entry != NULL; entry = entry->next) {
        const PostingNode *posting;

        for (posting = entry->postings; posting != NULL;
             posting = posting->next) {
            if (fprintf(file, "%s %d %d\n", entry->term, posting->doc_id,
                        posting->term_frequency) < 0) {
                fclose(file);
                return -1;
            }
        }
    }
    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

int index_load(InvertedIndex *index, const char *path)
{
    FILE *file;
    char magic[16];
    char word[INDEX_WORD_LEN];
    int version;
    int docs;
    int terms;
    int postings;
    int count = 0;
    int doc_id;
    int frequency;

    if (index == NULL || path == NULL) {
        return -1;
    }
    file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }
    if (fscanf(file, "%15s %d", magic, &version) != 2 ||
        strcmp(magic, "COMP1028_INDEX") != 0 || version != 1 ||
        fscanf(file, "%d %d %d", &docs, &terms, &postings) != 3 ||
        docs <= 0 || terms < 0 || postings < 0) {
        fclose(file);
        return -1;
    }

    index_free(index);
    index->document_count = docs;

    while (fscanf(file, "%127s %d %d", word, &doc_id, &frequency) == 3) {
        int i;

        if (doc_id < 0 || doc_id >= index->document_count || frequency <= 0) {
            fclose(file);
            index_free(index);
            return -1;
        }
        for (i = 0; i < frequency; i++) {
            if (index_add_occurrence(index, word, doc_id) != 0) {
                fclose(file);
                index_free(index);
                return -1;
            }
        }
        count++;
    }
    if (fscanf(file, "%127s", word) == 1) {
        fclose(file);
        index_free(index);
        return -1;
    }
    fclose(file);

    if (count != postings || index->unique_term_count != terms) {
        index_free(index);
        return -1;
    }
    return 0;
}
