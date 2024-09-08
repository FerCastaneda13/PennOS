/**
 * @file linked_list.c
 * @brief Implementation of linked list functions.
 *
 * This file contains the implementation of functions declared in linked_list.h. 
 * It includes functions for creating and manipulating a doubly linked list, such 
 * as adding, removing, and searching for nodes, as well as managing the list itself.
 */

#include "linked_list.h"
#include <stdlib.h>
#include <stdbool.h>
#include "errno.h"
#include <stdio.h>

LinkedList* create_list() {
    LinkedList* list = (LinkedList*)malloc(sizeof(LinkedList));
    if (list == NULL) {
        ERRNO = CREATE_LIST;
        p_perror("Failed to malloc list");
        perror("malloc");
        return NULL; // Failed to allocate memory for the new list
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

Node* create_node(void* data) {
    Node* node = (Node*)malloc(sizeof(Node));
    if (node == NULL) {
        ERRNO = CREATE_NODE;
        p_perror("Failed to malloc node");
        perror("malloc");
        return NULL; // Failed to allocate memory for the new node
    }
    
    node->data = data;
    node->next = NULL;
    node->prev = NULL;
    node->index = -1; // Index will be set when node is added to the list
    
    return node;
}

void insert_front(LinkedList* list, void* data) {
    Node* node = create_node(data);
    if (node == NULL) {
        ERRNO = INSERT_FRONT;
        p_perror("Failed to create node");
        return; // Failed to create node
    }
    
    node->next = list->head;
    node->index = 0; // The node will be at the front
    
    if (list->head != NULL) {
        list->head->prev = node;
        // Update indices of all other nodes
        Node* temp = list->head;
        while (temp != NULL) {
            temp->index++;
            temp = temp->next;
        }
    } else {
        list->tail = node; // If the list was empty, the new node is also the tail
    }
    
    list->head = node;
    list->size++;
}

void insert_end(LinkedList* list, void* data) {
    Node* node = create_node(data);
    if (node == NULL) {
        ERRNO = INSERT_END;
        p_perror("Failed to create node");
        return; // Failed to create node
    }
    
    node->prev = list->tail;
    node->index = list->size; // The node will be at the end
    
    if (list->tail != NULL) {
        list->tail->next = node;
    } else {
        list->head = node; // If the list was empty, the new node is also the head
    }
    list->tail = node;
    list->size++;
}

void insert_before(LinkedList* list, Node* referenceNode, Node* newNode) {
    if (list == NULL || referenceNode == NULL || newNode == NULL) {
        ERRNO = INSERT_BEFORE;
        p_perror("Failed to insert node");
        return; // Handle null pointers
    }

    newNode->next = referenceNode;
    newNode->prev = referenceNode->prev;

    if (referenceNode->prev != NULL) {
        referenceNode->prev->next = newNode;
    } else {
        list->head = newNode; // New node becomes the new head if reference node was the head
    }

    referenceNode->prev = newNode;
    list->size++;
}

void delete_node(LinkedList* list, Node* node) {
    if (node == NULL || list->head == NULL) {
//        ERRNO = DELETE_NODE;
//        p_perror("Failed to delete node");
        return; // Node doesn't exist or list is empty
    }
    
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        list->head = node->next; // Node is head
    }
    
    if (node->next != NULL) {
        node->next->prev = node->prev;
        // Update indices of subsequent nodes
        Node* temp = node->next;
        while (temp != NULL) {
            temp->index--;
            temp = temp->next;
        }
    } else {
        list->tail = node->prev; // Node is tail
    }
    
    // fine to free node
    free(node); 
    list->size--;
}

Node* get_fd_node(LinkedList* list, int data) {
    Node* current = list->head;
    while (current != NULL) {
        // Compare data pointers, does this work all the time?
        if (*(int*)current->data == data) {
            return current;
        }
        current = current->next;
    }
//    ERRNO = GET_NODE;
//    p_perror("Failed to get node");
    return NULL; // Data not found
}

Node* get_node(LinkedList* list, void* data) {
    Node* current = list->head;
    while (current != NULL) {
        // Compare data pointers, does this work all the time?
        if (current->data == data) {
            return current;
        }
        current = current->next;
    }
//    ERRNO = GET_NODE;
//    p_perror("Failed to get node");
    return NULL; // Data not found
}

Node* get_node_int(LinkedList* list, int pid) {
    Node* current = list->head;
    while (current != NULL) {
        // Compare data pointers, does this work all the time?
        if (*(int*)(current->data) == pid) {
            return current;
        }
        current = current->next;
    }
    ERRNO = GET_NODE_INT;
    p_perror("Failed to get node");
    return NULL; // Data not found
}

Node* get_node_by_index(LinkedList* list, int index) {
   if (index < 0 || index >= list->size) {
       return NULL; // Index out of bounds
   }

   Node* current = list->head;
   while (current != NULL && current->index != index) {
       current = current->next;
   }
   return current; // Will return NULL if index not found, should not happen
}

void free_list(LinkedList* list) {
    Node* current = list->head;
    while (current != NULL) {
        Node* next = current->next;
        free(current);
        current = next;
    }
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

bool move_head_to_end(LinkedList* list) {
  if (list == NULL || list->head == NULL || list->head == list->tail) {
        // List is empty or only has one element, no need to rotate
        return false;
    }

    Node* first = list->head;

    // Move head pointer to the next element in the list
    list->head = list->head->next;
    list->head->prev = NULL; // now the head's previous should be NULL

    // Adjust the tail
    list->tail->next = first; // old head becomes new tail
    first->prev = list->tail; // new tail's previous is old tail
    list->tail = first;       // update the tail pointer to the new tail
    list->tail->next = NULL;  // new tail's next should be NULL

    // Update indices if you are keeping track of them
    // This is an O(n) operation
    Node* current = list->head;
    int idx = 0;
    while (current != NULL) {
        current->index = idx++;
        current = current->next;
    }
    return true;
}

int get_index(LinkedList* list, void* data) {
    if (list == NULL || data == NULL) {
        return -1; // List or data is NULL
    }

    Node* current = list->head;
    int index = 0;
    while (current != NULL) {
        if (current->data == data) {
            return index; // Data found, return the index
        }
        current = current->next;
        index++;
    }

    return -1; // Data not found
}
