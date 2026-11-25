#ifndef COMP1028_UI_H
#define COMP1028_UI_H

#include "collection.h"
#include "index.h"

int ui_read_menu(int *out_choice);
int ui_read_query(char *buffer, int capacity);
int ui_read_top_k(int *out_k);
void ui_show_statistics(const DocumentCollenct *collection,
                        const InvertedIndex *index);

#endif /* COMP1028_UI_H */
