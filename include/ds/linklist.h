#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define list_is_empty(list) ((!((list)->begin)))

struct link_list_base_node;
struct link_list;

typedef struct link_list_base_node link_list_base_node_t;
typedef struct link_list link_list_t;

struct link_list_base_node{
    struct link_list_base_node* prev;
    struct link_list_base_node* next;
};

struct link_list{
    link_list_base_node_t * begin; // 指向开始的位置
    link_list_base_node_t * end; // 指向最后一个位置
    link_list_base_node_t * now; // 指向链表中的区域，给用户使用
};

#include "core/ping_alloc.h"

/* Interface */
#define list_insert_front(l, n) _list_insert_front(l, (link_list_base_node_t *)n)

#define list_insert_back(l, n) _list_insert_back(l, (link_list_base_node_t *)n)

// 遍历整个链表找到node 在node节点后插入n这个节点
#define list_insert_after(l, n, node) _list_insert_after(l, (link_list_base_node_t *)n, (link_list_base_node_t *) node) 

// 遍历整个链表找到node 在node节点前插入n这个节点
#define list_insert_before(l, n, node) _list_insert_before(l, (link_list_base_node_t *)n, (link_list_base_node_t *) node)

#define list_delete(l, node) _list_delete(l, (link_list_base_node_t *) node) 

#define link_node_delete(n) _link_node_delete((link_list_base_node_t *) n)

/* This file was automatically generated.  Do not edit! */
bool _list_delete(link_list_t *l,link_list_base_node_t *node);
bool list_delete_back(link_list_t *l);
bool list_delete_front(link_list_t *l);
bool _list_insert_before(link_list_t *l,link_list_base_node_t *n,link_list_base_node_t *node);
bool _list_insert_after(link_list_t *l,link_list_base_node_t *n,link_list_base_node_t *node);
bool _list_insert_back(link_list_t *l,link_list_base_node_t *n);
bool _list_insert_front(link_list_t *l,link_list_base_node_t *n);
void _link_node_delete(link_list_base_node_t *n);


void link_list_init(link_list_t *l);
void link_list_free(link_list_t *l);