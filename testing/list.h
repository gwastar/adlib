#pragma once

#include <stddef.h>
#include <stdint.h>

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

static void list_push_tail(struct list_head *list, struct list_head *item)
{
	item->prev = list->prev;
	item->next = list;
	list->prev->next = item;
	list->prev = item;
}

static void list_remove_item(struct list_head *item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;
	item->prev = (struct list_head *)(uintptr_t)0xdeadbeef;
	item->next = (struct list_head *)(uintptr_t)0xdeadbeef;
}

static struct list_head *list_pop_head(struct list_head *list)
{
	if (list_empty(list)) {
		return NULL;
	}
	struct list_head *item = list->next;
	list_remove_item(item);
	return item;
}

#define list_foreach(list, itername)					\
	for (struct list_head *itername = (list)->next; itername != (list); itername = itername->next)
