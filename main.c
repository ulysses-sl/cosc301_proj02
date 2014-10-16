#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <signal.h>
#include "list.h"

char** tokenify(const char *s);
void commandify(const char *s, struct node **list);
void error_print_tokens(char **tokens);
void append_tokens(struct node **list, char **tokens);
void free_tokens(char **tokens);
void free_list(struct node **list);
bool pop_list(struct node **list);
bool mode_set(char **tokens, bool current_mode);

/* Create list of array of tokens and ignore anything after # */
char** tokenify(const char *s) {
    int len = strlen(s);

    // token position and length flag
    int* token_flag = malloc((len + 1) * sizeof(int));
    int token_count = 0; // the total num of tokens
    int counter = 0;    // temp counter for token length

    // marks beginning of each token with token length
    for (int i = len - 1; i >= 0; i--) {
        // count every non-space for each token length
        if (!isspace(s[i])) { counter++;}

        // create flags for start of each token, containing its length
        if (counter > 0 && (i == 0 || isspace(s[i-1]))) {
            token_flag[i] = counter;
            counter = 0;
            token_count++;
        }
        else {
            // this is not the index you are looking for
            token_flag[i] = 0;
        }
    }

    // the tokens to return
    char** tokens = malloc((token_count + 1) * sizeof(char*));

    int j = 0;

    // assign tokens for each token pointer
    for (int i = 0; i < token_count; i++) {
        while (token_flag[j] < 1) { j++; }
        int token_len = token_flag[j];  // length of each token

        // allocate each token space
        tokens[i] = strndup(s + j, token_len);
        j++;
    }

    tokens[token_count] = NULL;

    free(token_flag);

    return tokens;
}


/* split per each ';', and stop at # or end of string
 * then tokenify each split command and append to linked list
 * */
void commandify(const char *s, struct node **list) {
    int i = -1;
    int j = 0;
    while (1) {
        if (s[j] == ';' || s[j] == '#' || s[j] == '\n' || s[j] == '\0') {
            if (j - i > 1) {
                char *to_be_token = strndup(s + i + 1, j - i - 1);
                append_tokens(list, tokenify(to_be_token));
                free(to_be_token);
            }
            i = j;
        }
        if (s[j] == '#' || s[j] == '\n' || s[j] == '\0') { break; }
        j++;
    }
}


/* print all the tokens in one line */
void error_print_tokens(char **tokens) {
    fprintf(stderr, "Cannot execute: ");
    int i = 0;
    while (tokens[i] != NULL) { fprintf(stderr, "%s ", tokens[i++]); }
    fprintf(stderr, "\n");
}


/* append each array of tokens to the end of a linked list */
void append_tokens(struct node **list, char **tokens) {
    // ignore empty token
    if (tokens[0] == NULL) { free(tokens); return; }
/*
    int i = 0;
    while (tokens[i] != NULL) {
        printf("%s ", tokens[i++]);
    }
    printf("\n");
*/
    struct node *newnode = malloc(sizeof(struct node));
    newnode->next = NULL;
    newnode->tokens = tokens;
    if (*list == NULL) { *list = newnode; }
    else {
        struct node *temp = *list;
        while (temp->next != NULL) { temp = temp->next; }
        temp->next = newnode;
    }
}


/* free the array of tokens */
void free_tokens(char **tokens) {
    int i = 0;
    while (tokens[i] != NULL) { free(tokens[i++]); }
    free(tokens);
}


/* free the list of arrays of tokens */
void free_list(struct node **list) {
    while (pop_list(list));
}


/* pop the top item of the list, return true if success */
bool pop_list(struct node **list) {
    if (*list != NULL) {
        struct node *temp = *list;
        *list = (*list)->next;
        free_tokens(temp->tokens);
        free(temp);
        return true;
    }
    return false;
}


bool mode_set(char **tokens, bool current_mode) {
    if (tokens[1] == NULL) {
        if (current_mode) {
            printf("Mode: Parallel\n");
        }
        else {
            printf("Mode: Sequential\n");
        }
        return current_mode;
    }
    else if (tokens[2] != NULL) {
        printf("** Unrecognized mode argument **\n");
        return current_mode;
    }
    else if (strcmp(tokens[1], "sequential") == 0 ||
             strcmp(tokens[1], "s") == 0) {
        return false;
    }
    else if (strcmp(tokens[1], "parallel") == 0 ||
             strcmp(tokens[1], "p") == 0) {
        return true;
    }
    else {
        printf("** Unrecognized mode argument **\n");
        return current_mode;
    }
}


int main(int argc, char **argv) {
    bool parallel_mode = false;
    bool exit_flag = false;
    bool next_parallel_mode = false;
    int status = 0;
    pid_t pid = 0;
    struct node *list = NULL;

    while (1) {
        // input buffer
        char *input = malloc(1024 * sizeof(char));
        strcpy(input, "");

        printf("lleh$ ");
        fgets(input, 1024, stdin);

        int len = strlen(input);
        input[len - 1] = '\0';
        commandify(input, &list);
        free(input);

        while (list != NULL) {
            if (strcmp(list->tokens[0], "exit") == 0) {
                exit_flag = true;
                pop_list(&list);
            }
            else if (strcmp(list->tokens[0], "mode") == 0) {
                next_parallel_mode = mode_set(list->tokens, parallel_mode);
                pop_list(&list);
            }
            /*else if (strcmp(list->tokens[0], "jobs") == 0) {
            }
            else if (strcmp(list->tokens[0], "pause") == 0) {
            }
            else if (strcmp(list->tokens[0], "resume") == 0) {
            }*/
            else {
                pid = fork();
                if (pid == 0) {
                    if (execv(list->tokens[0], list->tokens) < 0) {
                        error_print_tokens(list->tokens);
                        free_list(&list);
                        exit(0);
                    }
                }
                else if (!parallel_mode) {
                    wait(&status);
                }
                pop_list(&list);
            }
        }
        while (wait(&status) != -1);
        if (exit_flag) {
            exit(0);
        }
        parallel_mode = next_parallel_mode;
    }

    return 0;
}

