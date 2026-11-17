#include <stdio.h>
#include <ctype.h>

#define MAX_WORD 64

int next_word(FILE *input, char *word, int capacity) {
    int  ch;        
    int  len = 0;
    int  in_word = 0;

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

int main(void) {
    FILE *fp = fopen("data/corpus/alt.atheism_49960.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "\n");
        return 1;
    }

    char word[MAX_WORD];
    while (next_word(fp, word, MAX_WORD) == 1) {
        printf("%s\n", word);
    }

    fclose(fp);
    return 0;
}