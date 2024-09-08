/**
 * @file linked_list.h
 * @brief Header file for the PennOS generic linked list, which allows this to be used for many types of structs.
 */

// Created by Omar Ameen on 11/11/23.
#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stddef.h>
#include <stdbool.h>
#include "../kernel/PCB.h"
#include "signal.h"
#include "../kernel/scheduler.h"

/// Forward declaration of PCB
typedef struct PCB PCB;

/// Define a node in the doubly linked list
typedef struct Node {
    void* data; ///< Pointer to data held in the node
    struct Node* next; ///< Pointer to the next node in the list
    struct Node* prev; ///< Pointer to the previous node in the list
    int index; ///< Index of the node in the list
} Node;

/// Define the doubly linked list structure
typedef struct LinkedList {
    Node* head; ///< Pointer to the head of the list
    Node* tail; ///< Pointer to the tail of the list
    size_t size; ///< Number of elements in the list
} LinkedList;

/// Initialize the doubly linked list
LinkedList* create_list();

/// Create a new node with the given data
Node* create_node(void* data);

/// Insert a node at the front of the list
void insert_front(LinkedList* list, void* data);

/// Insert a node at the end of the list
void insert_end(LinkedList* list, void* data);

/// Insert a node before the referenceNode
void insert_before(LinkedList* list, Node* referenceNode, Node* newNode);

/// Insert a node after the referenceNode
void insert_after(LinkedList* list, Node* referenceNode, Node* newNode);

/// Delete a node from the list
void delete_node(LinkedList* list, Node* node);

/// Find a node in the list with the given data
Node* get_node(LinkedList* list, void* data);

Node* get_fd_node(LinkedList* list, int data);

/// Find a node in the list with given pid int
Node* get_node_int(LinkedList* list, int pid);

/// Find a node in the list with the given data and return its index
int get_index(LinkedList* list, void* data);

/// Get the node at the given index
Node* get_node_by_index(LinkedList* list, int index);

/// Free the list and all its nodes
void free_list(LinkedList* list);

/// Move the head to the end of the list
bool move_head_to_end(LinkedList* list);

#endif // LINKED_LIST_H