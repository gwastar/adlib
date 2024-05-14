#ifndef __list_include__
#define __list_include__

#include <stdbool.h>
#include <stddef.h>

#ifndef container_of
// from wikipedia, the ternary operator forces matching types on pointer and member
#define container_of(ptr, type, member)					\
	((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))
#endif

struct list_head {
	struct list_head *prev;
	struct list_head *next;
};

static struct list_head list_head_init(struct list_head *list)
{
	list->prev = list;
	list->next = list;
	return *list;
}

static bool list_empty(struct list_head *list)
{
	return list->next == list;
}

static void list_insert_after(struct list_head *pos, struct list_head *item)
{
	item->prev = pos;
	item->next = pos->next;
	pos->next->prev = item;
	pos->next = item;
}

static void list_insert_before(struct list_head *pos, struct list_head *item)
{
	item->prev = pos->prev;
	item->next = pos;
	pos->prev->next = item;
	pos->prev = item;
}

static void list_remove_item(struct list_head *item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;
	item->prev = (struct list_head *)0xdeadbeef;
	item->next = (struct list_head *)0xdeadbeef;
}

#define list_foreach(list, itername)					\
	for (struct list_head *itername = (list)->next, *___itername__##next = itername->next; itername != (list); itername = ___itername__##next, ___itername__##next = itername->next)

#define list_foreach_elem(list, itername, type, member)			\
	for (type *itername = container_of((list)->next, type, member), *___itername__##next = container_of(itername->member.next, type, member); &itername->member != (list); itername = ___itername__##next, ___itername__##next = container_of(itername->member.next, type, member))

#endif
