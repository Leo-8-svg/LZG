#include "tokenizer.h"
#include <ctype.h>
#include <string.h>

int next_word(FILE *input, char *word, int capacity) {
    int ch;
    int len = 0;
    int in_word = 0;

    while ((ch = fgetc(input)) != EOF) {
        if (isalpha((unsigned char)ch)) {
            if (len < capacity - 1) {
                word[len++] = tolower((unsigned char)ch);
            }
            in_word = 1;
        } else {
            if (in_word) {
                word[len] = '\0';
                return 1;
            }
        }
    }

    if (ferror(input)) return -1;

    if (in_word) {
        word[len] = '\0';
        return 1;
    }
    return 0;
}

int load_documents(const char *list_path, Document *docs, int max_docs) {
    FILE *list = fopen(list_path, "r");
    if (list == NULL) {
        fprintf(stderr, "Warning: cannot open list %s\n", list_path);
        return -1;
    }

    char path[PATH_LEN];
    char word[MAX_WORD];
    int n = 0;

    while (fgets(path, sizeof(path), list) != NULL) {
        path[strcspn(path, "\r\n")] = '\0';
        if (path[0] == '\0') continue;

        FILE *fp = fopen(path, "r");
        if (fp == NULL) {
            fprintf(stderr, "Warning: cannot open %s\n", path);
            continue;
        }

        docs[n].id = n;
        strncpy(docs[n].filename, path, PATH_LEN - 1);
        docs[n].filename[PATH_LEN - 1] = '\0';
        docs[n].word_count = 0;

        while (next_word(fp, word, MAX_WORD) == 1) {
            docs[n].word_count++;
        }

        fclose(fp);
        n++;
        if (n >= max_docs) break;
    }

    fclose(list);
    return n;
}