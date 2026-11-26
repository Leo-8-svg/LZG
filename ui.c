#include "ui.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void consume_rest_of_line(FILE *stream) {
    int ch;
    while ((ch = fgetc(stream)) != EOF) {
        if (ch == '\n') {
            break;
        }
    }
}

/* 返回值：1 表示成功读入一行；0 表示遇到 EOF；-1 表示超长或读取错误 */
static int read_line_strict(char *buf, size_t size, int *was_truncated) {
    if (was_truncated != NULL) {
        *was_truncated = 0;
    }
    if (buf == NULL || size == 0) {
        return -1;
    }
     if (fgets(buf, (int)size, stdin) == NULL) {
    return ferror(stdin) ? -1 : 0;
}

    char *nl = strchr(buf, '\n');
     int ch = fgetc(stdin);
if (ch == '\n') {
    return 1;
}
if (ch == EOF) {
    return ferror(stdin) ? -1 : 1;
}

    if (was_truncated != NULL) {
        *was_truncated = 1;
    }
    consume_rest_of_line(stdin);
    return -1;
}

/* 去除首尾空白字符；若处理后为空串则返回 1，否则返回 0 */
static int trim_and_check_blank(char *str) {
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }

    size_t start = 0;
    while (str[start] != '\0' && isspace((unsigned char)str[start])) {
        start++;
    }
    if (start > 0) {
        memmove(str, str + start, len - start + 1);
    }

    return str[0] == '\0' ? 1 : 0;
}

/* 严格解析整行整数：允许前后空白，不允许尾随非空白字符或溢出 */
static int parse_strict_int(const char *s, int *out_val) {
    if (s == NULL || out_val == NULL) {
        return -1;
    }

    const char *p = s;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p == '\0') {
        return -1;
    }

    errno = 0;
    char *endptr = NULL;
    long val = strtol(p, &endptr, 10);
    if (errno == ERANGE || endptr == p || val < INT_MIN || val > INT_MAX) {
        return -1;
    }

    while (*endptr != '\0') {
        if (!isspace((unsigned char)*endptr)) {
            return -1;
        }
        endptr++;
    }

    *out_val = (int)val;
    return 0;
}

int ui_read_menu(int *out_choice) {
    if (out_choice == NULL) {
        return -1;
    }

    printf("\n=== Document Search Menu ===\n");
    printf("1. Load Documents\n");
    printf("2. Build Index\n");
    printf("3. Search\n");
    printf("4. Show Statistics\n");
    printf("5. Save Report\n");
    printf("6. Save Index\n");
    printf("7. Load Index\n");
    printf("0. Exit\n");
    printf("Enter choice: ");
    fflush(stdout);

    char line[64];
    int truncated = 0;
    int rc = read_line_strict(line, sizeof(line), &truncated);
    if (rc == 0) {
        return 0;
    }
    if (rc < 0 || truncated) {
        return -1;
    }

    int choice = 0;
    if (parse_strict_int(line, &choice) != 0) {
        return -1;
    }
    if (choice < 0 || choice > 7) {
        return -1;
    }

    *out_choice = choice;
    return 1;
}

int ui_read_query(char *buffer, int capacity) {
    if (buffer == NULL || capacity <= 1) {
        return -1;
    }

    printf("Enter search query: ");
    fflush(stdout);

    int truncated = 0;
    int rc = read_line_strict(buffer, (size_t)capacity, &truncated);
    if (rc == 0) {
        return 0;
    }
    if (rc < 0 || truncated) {
        return -1;
    }
    if (trim_and_check_blank(buffer)) {
        return -1;
    }

    return 1;
}

int ui_read_top_k(int *out_k) {
    if (out_k == NULL) {
        return -1;
    }

    printf("Enter Top K (1..%d): ", MAX_DOCS);
    fflush(stdout);

    char line[64];
    int truncated = 0;
    int rc = read_line_strict(line, sizeof(line), &truncated);
    if (rc == 0) {
        return 0;
    }
    if (rc < 0 || truncated) {
        return -1;
    }

    int k = 0;
    if (parse_strict_int(line, &k) != 0) {
        return -1;
    }
    if (k < 1 || k > MAX_DOCS) {
        return -1;
    }

    *out_k = k;
    return 1;
}

void ui_show_statistics(const DocumentCollenct *collection,
                        const InvertedIndex *index) {
    int doc_count = (collection != NULL) ? collection->count : 0;
    long long total_words = 0;

    printf("\n--- Statistics ---\n");
    printf("Documents: %d\n", doc_count);

    if (collection != NULL && collection->documents != NULL && doc_count > 0) {
        for (int i = 0; i < doc_count; i++) {
            int wc = collection->documents[i].word_count;
            total_words += wc;
            printf("  Doc %d (%s): %d words\n",
                   collection->documents[i].id,
                   collection->documents[i].filename,
                   wc);
        }
    }

    printf("Total words: %lld\n", total_words);

    if (index != NULL) {
        int posting_count = index_posting_count(index);
        printf("Unique terms: %d\n", index->unique_term_count);
        printf("Total postings: %d\n", posting_count);
    } else {
        printf("Unique terms: 0\n");
        printf("Total postings: 0\n");
    }
}
