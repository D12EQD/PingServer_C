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

typedef struct link_list_base_node linkListNode;
typedef struct link_list linkList;

struct link_list_base_node{
    struct link_list_base_node* prev;
    struct link_list_base_node* next;
};

struct link_list{
    linkListNode * begin; // 指向开始的位置
    linkListNode * end; // 指向最后一个位置
    linkListNode * now; // 指向链表中的区域，给用户使用
};

/* Interface */
#define list_insert_front(l, n) _list_insert_front(l, (linkListNode *)n)
#define list_insert_back(l, n) _list_insert_back(l, (linkListNode *)n)

// 遍历整个链表找到node 在node节点后插入n这个节点
#define list_insert_after(l, n, node) _list_insert_after(l, (linkListNode *)n, (linkListNode *) node) 

// 遍历整个链表找到node 在node节点前插入n这个节点
#define list_insert_before(l, n, node) _list_insert_before(l, (linkListNode *)n, (linkListNode *) node)
#define list_delete(l, node) _list_delete(l, (linkListNode *) node) 
#define link_node_delete(l, n) _link_node_delete((linkList *)l, (linkListNode *) n)

/* This file was automatically generated.  Do not edit! */
bool _list_delete(linkList *l,linkListNode *node);
bool list_delete_back(linkList *l);
bool list_delete_front(linkList *l);
bool _list_insert_before(linkList *l,linkListNode *n,linkListNode *node);
bool _list_insert_after(linkList *l,linkListNode *n,linkListNode *node);
bool _list_insert_back(linkList *l,linkListNode *n);
bool _list_insert_front(linkList *l,linkListNode *n);
void _link_node_delete(linkList*l, linkListNode *n);

void link_list_init(linkList *l);
void link_list_free(linkList *l);