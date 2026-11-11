#include <stdio.h>
#include <ctype.h>

#define MAX_WORD 64

int main(void) {
    const char *text = "Solar, ENERGY! energy 123 wind-energy";
    char word[MAX_WORD];
    int  len = 0;

    for (int i = 0; text[i] != '\0'; i++) {
        if (isalpha((unsigned char)text[i])) {
            if (len < MAX_WORD - 1) {
                word[len++] = tolower((unsigned char)text[i]);
            }
        } else {
            if (len > 0) {
                word[len] = '\0';
                printf("%s\n", word);
                len = 0;
            }
        }
    }

    if (len > 0) {
        word[len] = '\0';
        printf("%s\n", word);
    }

    return 0;
}
