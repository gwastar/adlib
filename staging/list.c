#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "list.h"
#include "array.h"

struct item {
	struct list_head link;
	int i;
};

static int *arr = NULL;
static struct list_head list;

static void add(int i)
{
	array_add(arr, i);
	struct item *item = malloc(sizeof(*item));
	item->i = i;
	list_insert_before(&list, &item->link);
}

static void remove(int i)
{
	array_fori(arr, k) {
		if (arr[k] == i) {
			array_ordered_delete(arr, k);
			break;
		}
	}

	list_foreach_elem(&list, item, struct item, link) {
		if (item->i == i) {
			list_remove_item(&item->link);
			break;
		}
	}
}

static void check(void)
{
	int *iter;
	struct list_head *cur = list.next;
	array_foreach(arr, iter) {
		assert(cur != &list);
		assert(*iter == container_of(cur, struct item, link)->i);
		cur = cur->next;
	}
}

int main(void)
{
	list_head_init(&list);

	srand(time(NULL));

	for (unsigned int i = 0; i < 100000; i++) {
		int r = rand() % 64;
		if (r & 1) {
			add(r);
		} else {
			remove(r);
		}
		check();
	}
}
