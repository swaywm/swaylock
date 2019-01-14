#ifndef _SWAY_LIST_H
#define _SWAY_LIST_H

typedef struct {
	int capacity;
	int length;
	void **items;
} list_t;

list_t *create_list(void);
void list_free(list_t *list);
void list_add(list_t *list, void *item);
void list_insert(list_t *list, int index, void *item);
void list_del(list_t *list, int index);

/* Calls `free` for each item in the list, then frees the list.
 * Do not use this to free lists of primitives or items that require more
 * complicated deallocation code.
 */
void list_free_items_and_destroy(list_t *list);
#endif
