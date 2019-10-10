/*
 * Linked lists support
 *
 * Copyright (C) 2002 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __WINE_SERVER_LIST_H
#define __WINE_SERVER_LIST_H

struct list
{
    struct list *next;
    struct list *prev;
};

/* add element at the head of the list */
inline static void list_add_head( struct list *list, struct list *elem )
{
    elem->next = list->next;
    elem->prev = list;
    list->next->prev = elem;
    list->next = elem;
}

/* add element at the tail of the list */
inline static void list_add_tail( struct list *list, struct list *elem )
{
    elem->next = list;
    elem->prev = list->prev;
    list->prev->next = elem;
    list->prev = elem;
}

/* remove an element from its list */
inline static void list_remove( struct list *elem )
{
    elem->next->prev = elem->prev;
    elem->prev->next = elem->next;
}

/* get the next element */
inline static struct list *list_next( struct list *list, struct list *elem )
{
    struct list *ret = elem->next;
    if (elem->next == list) ret = NULL;
    return ret;
}

/* get the previous element */
inline static struct list *list_prev( struct list *list, struct list *elem )
{
    struct list *ret = elem->prev;
    if (elem->prev == list) ret = NULL;
    return ret;
}

/* get the first element */
inline static struct list *list_head( struct list *list )
{
    return list_next( list, list );
}

/* get the last element */
inline static struct list *list_tail( struct list *list )
{
    return list_prev( list, list );
}

/* check if a list is empty */
inline static int list_empty( struct list *list )
{
    return list->next == list;
}

/* initialize a list */
inline static void list_init( struct list *list )
{
    list->next = list->prev = list;
}

/* iterate through the list */
#define LIST_FOR_EACH(cursor,list) \
    for ((cursor) = (list)->next; (cursor) != (list); (cursor) = (cursor)->next)

/* macros for statically initialized lists */
#define LIST_INIT(list)  { &(list), &(list) }

/* get pointer to object containing list element */
#define LIST_ENTRY(elem, type, field) \
    ((type *)((char *)(elem) - (unsigned int)(&((type *)0)->field)))

#endif  /* __WINE_SERVER_LIST_H */
