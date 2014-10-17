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
void add_process(struct process **plist, int pid, char *command);
bool clear_finished_process(struct process **plist);
bool mode_set(char **tokens, bool current_mode);
void exit_message(int pid, char *command);
void process_running(int pid, bool state, struct process *plist);


/* return tokens of parsed paths */
char** parse_path(FILE *input_file) {
    char **rv = NULL;
    char *content = malloc(1000 * sizeof(char));
    if (fgets(content, 1000, input_file) != NULL) {
        rv = tokenify(content);
    }
    free(content);
    return rv;
}


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
    fflush(stderr);
}


/* append each array of tokens to the end of a linked list */
void append_tokens(struct node **list, char **tokens) {
    // ignore empty token
    if (tokens[0] == NULL) { free(tokens); return; }
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
    if (tokens == NULL) { return; }
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


/* add new process to the list */
void add_process(struct process **plist, int pid, char *command) {
    struct process *newp = malloc(sizeof(struct process));
    strncpy(newp->name, command, 20);
    newp->pid = pid;
    newp->state = true;
    newp->next = NULL;
    if (plist != NULL) { newp->next = *plist; }
    *plist = newp;
}


/* if there is any finished process, remove and return true */
bool clear_finished_process(struct process **plist) {
    int status;
    if (*plist == NULL) { return false; }

    if (waitpid((*plist)->pid, &status, WNOHANG) != 0) {
        exit_message((*plist)->pid, (*plist)->name);
        struct process *temp = *plist;
        *plist = (*plist)->next;
        free(temp);
        return true;
    }
    struct process *curr = *plist;
    while (curr->next != NULL) {
        if (waitpid(curr->next->pid, &status, WNOHANG) != 0) {
            exit_message(curr->next->pid, curr->next->name);
            struct process *temp = curr->next;
            curr->next = curr->next->next;
            free(temp);
            return true;
        }
        curr = curr->next;
    }
    return false;
}


void exit_message(int pid, char *command) {
    printf("\nProcess %d (%s) has finished.\n", pid, command);
}


void jobprint(struct process *plist) {
    if (plist == NULL) {
        printf("No job running in the background\n");
        return;
    }
    while (plist != NULL) {
        if (plist->state) {
            printf("Process %d (%s) is running.\n", plist->pid, plist->name);
        }
        else {
            printf("Process %d (%s) is paused.\n", plist->pid, plist->name);
        }
        plist = plist->next;
    }
}


/* set and display mode */
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
        fprintf(stderr, "** Unrecognized mode argument **\n");
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
        fprintf(stderr, "** Unrecognized mode argument **\n");
        return current_mode;
    }
}


/* update process state */
void process_running(int pid, bool state, struct process *plist) {
    while (plist != NULL) {
        if (plist->pid == pid) {
            plist->state = state;
            return;
        }
        plist = plist->next;
    }
    fprintf(stderr, "No process with the pid running\n");
}


int main(int argc, char **argv) {
    FILE *pathfile = NULL;
    char** path_list = NULL;
    struct stat statbuf;

    if (stat("shell-config", &statbuf) == 0) {
        pathfile = fopen("shell-config", "r");
        if (pathfile != NULL) {
            path_list = parse_path(pathfile);
            fclose(pathfile);
        }
    }
    
    bool parallel_mode = false;
    bool exit_flag = false;
    bool next_parallel_mode = false;
    bool new_cursor = true;
    int status = 0;
    pid_t pid = 0;
    struct node *list = NULL;
    struct process *process_list = NULL;

    while (1) {
        struct pollfd pfd[1];
        pfd[0].fd = 0;
        pfd[0].events = POLLIN;
        pfd[0].revents = 0;

        // input buffer
        char *input = malloc(1024 * sizeof(char));
        strcpy(input, "");

        if (new_cursor) {
            printf("lleh$ ");
            fflush(stdout);
            new_cursor = false;
        }

        int rv = poll(&pfd[0], 1, 1000);

        if (rv > 0) {
            if (fgets(input, 1024, stdin) != NULL) {
                int len = strlen(input);
                input[len - 1] = '\0';
                commandify(input, &list);
                new_cursor = true;
            }
            else {
                exit_flag = true;
                list = NULL;
            }
        }
        else {
            list = NULL;
        }


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
            else if (strcmp(list->tokens[0], "jobs") == 0) {
                jobprint(process_list);
                pop_list(&list);
            }
            else if (strcmp(list->tokens[0], "pause") == 0) {
                if (list->tokens[1] == NULL ||
                    list->tokens[2] != NULL) {
                    fprintf(stderr, "Unrecognized pause argument\n");
                }
                else {
                    int pid = strtol(list->tokens[1], NULL, 10);
                    if (pid == 0) {
                        fprintf(stderr, "Unrecognized pause argument\n");
                    }
                    else {
                        kill(pid, SIGSTOP);
                        process_running(pid, false, process_list);
                    }
                }
                pop_list(&list);
            }
            else if (strcmp(list->tokens[0], "resume") == 0) {
                if (list->tokens[1] == NULL ||
                    list->tokens[2] != NULL) {
                    fprintf(stderr, "Unrecognized resume argument\n");
                }
                else {
                    int pid = strtol(list->tokens[1], NULL, 10);
                    if (pid == 0) {
                        fprintf(stderr, "Unrecognized resume argument\n");
                    }
                    else {
                        kill(pid, SIGCONT);
                        process_running(pid, true, process_list);
                    }
                }
                pop_list(&list);
            }
            else {
                // we actually get to execute something
                int commlen = strlen(list->tokens[0]) + 1;
                char *command = malloc(commlen * sizeof(char));
                strcpy(command, list->tokens[0]);
                int curr_path = 0;

                bool it_runs = false;

                // match the path-file pair puzzle
                while (path_list != NULL &&
                       path_list[curr_path] != NULL &&
                       stat(command, &statbuf) != 0) {
                    free(command);
                    commlen = strlen(path_list[curr_path]) +
                              strlen(list->tokens[0]) + 2;
                    command = malloc(commlen);
                    strcpy(command, path_list[curr_path++]);
                    strcat(command, "/");
                    strcat(command, list->tokens[0]);
                }

                // if command exists, swap.
                if (stat(command, &statbuf) == 0) {
                    free(list->tokens[0]);
                    list->tokens[0] = command;
                    it_runs = true;
                }
                else { free(command); }

                // ...and fork, finally
                pid = fork();
                if (pid == 0) {

                    if (execv(list->tokens[0], list->tokens) < 0) {
                        error_print_tokens(list->tokens);
                        free_tokens(path_list);
                        free_list(&list);
                        while (process_list != NULL) {
                            struct process *ptemp = process_list;
                            process_list = process_list->next;
                            free(ptemp);
                        }
                        exit(0);
                    }
                }
                else if (!parallel_mode) {
                    wait(&status);
                }
                else if (it_runs) {
                    add_process(&process_list, pid, list->tokens[0]);
                    it_runs = false;
                }
                pop_list(&list);
            }
        }
        if(clear_finished_process(&process_list)) {
            new_cursor = true;
        }

        if (exit_flag) {
            if (process_list == NULL) {
                printf("\n");
                free_tokens(path_list);
                exit(0);
            }
            else {
                fprintf(stderr, "\nCannot terminate: background processes running\n");
                exit_flag = false;
                new_cursor = true;
            }
        }
    
        if (parallel_mode != next_parallel_mode && process_list != NULL) {
            fprintf(stderr, "Cannot change mode: background processes running\n");
            next_parallel_mode = parallel_mode;
            new_cursor = true;
        }
        else {
            parallel_mode = next_parallel_mode;
        }
    }

    return 0;
}

