#ifndef __LIST_H__
#define __LIST_H__

/* your list data structure declarations */

struct node {
    char** tokens;
    struct node *next;
};

struct process {
    char name[20];
    int pid;
    bool state;
    struct process *next;
};

#endif // __LIST_H__
