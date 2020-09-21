//
// Created by Arthur Miao on 27/4/20.
//
#include "svc.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct commit commit;
typedef struct change change;
typedef struct file file;
typedef struct branch branch;

void free_commit(commit*);
void detach_commit(commit*, commit*);
void get_commit_id(commit*);
void insert_sort();
void encode_and_copy(char*, char*, int);

struct commit {
    char *id;
    char *message;
    int *branches;
    commit *prev;
    commit *from;
    change *changes;
    file *files;
    int n_branches;
    int n_changes;
    int n_files;
    int in_branch;
};

struct change {
    char *file_name;
    char *operation;
};

struct file {
    char *name;
    int hash;
};

struct branch {
    char *name;
    commit *begin;
    commit *head;
};

commit *HEAD = NULL;
change *changes = NULL;
branch *branches = NULL;
int n_changes = 0;
int n_branches = 0;
int curr_branch = 0;
commit **detached_commit = NULL;
int n_detached = 0;
int printing = 0;

void get_commit_id(commit *com) {
    int id = 0;
    for(int i = 0; i < strlen(com->message); i++) {
        id = (id + (int)(com->message[i])) % 1000;
    }
    for(int i = 0; i < com->n_changes; i++) {
        if(!strcmp(com->changes[i].operation, "add")) {
            id += 376591;
        } else if(!strcmp(com->changes[i].operation, "del")) {
            id += 85973;
        } else {
            id += 9573681;
        }
        for(int j = 0; j < strlen(com->changes[i].file_name); j++) {
            id = (id * ((int)(com->changes[i].file_name[j]) % 37)) % 15485863 + 1;
        }
    }
    com->id = (char*)malloc(7);
    sprintf(com->id, "%06x", id);
}

void insert_sort() {
    change ch = changes[n_changes - 1];
    for(int i = 0; i < n_changes - 1; i++) {
        if(strcasecmp(ch.file_name, changes[i].file_name) < 0) {
            for(int j = n_changes - 1; j > i; j--) {
                changes[j] = changes[j-1];
            }
            changes[i] = ch;
            break;
        }
    }
}

void *svc_init(void) {
    // TODO: Implement
    branches = (branch*)realloc(branches, sizeof(branch) * ++n_branches);
    branches[0].name = strdup("master");

    commit *com = (commit*)malloc(sizeof(commit));
    com->id = NULL;
    com->message = NULL;
    com->branches = NULL;
    com->prev = NULL;
    com->from = NULL;
    com->changes = NULL;
    com->files = NULL;
    com->n_branches = 0;
    com->n_changes = 0;
    com->n_files = 0;
    com->in_branch = 0;

    HEAD = com;
    branches[0].begin = branches[0].head = com;
    return NULL;
}

void free_commit(commit *ptr) {
    if(ptr->id) {
        free(ptr->id);
        ptr->id = NULL;
    }
    if(ptr->message) {
        free(ptr->message);
        ptr->message = NULL;
    }
    if(ptr->n_branches) {
        free(ptr->branches);
        ptr->n_branches = 0;
    }
    if(ptr->n_changes) {
        for(int i = 0; i < ptr->n_changes; i++) {
            free(ptr->changes[i].file_name);
        }
        free(ptr->changes);
        ptr->n_changes = 0;
    }
    if(ptr->n_files) {
        for(int i = 0; i < ptr->n_files; i++) {
            free(ptr->files[i].name);
        }
        free(ptr->files);
        ptr->n_files = 0;
    }
    free(ptr);
}

void cleanup(void *helper) {
    // TODO: Implement
    for(int i = n_branches - 1; i >= 0; i--) {
        commit *ptr = branches[i].head;
        while(ptr) {
            if(ptr->in_branch != i) break;
            commit *prev = ptr->prev;
            free_commit(ptr);
            ptr = prev;
        }
        free(branches[i].name);
    }
    free(branches);
    for(int i = 0; i < n_detached; i++) {
        free_commit(detached_commit[i]);
    }
    free(detached_commit);
}

int hash_file(void *helper, char *file_path) {
    // TODO: Implement
    if(!file_path) return -1;
    FILE *fp = fopen(file_path, "r");
    if(!fp) return -2;

    int hash = 0;
    for(int i = 0; i < strlen(file_path); i++) {
        hash = (hash + (int)(file_path[i])) % 1000;
    }
    int c;
    while((c = fgetc(fp)) != EOF) {
        hash = (hash + c) % 2000000000;
    }
    fclose(fp);
    return hash;
}

void encode_and_copy(char *sour_id, char *sour_file_name, int type) {
    char *dest_path = (char*)malloc(strlen(sour_id) + strlen(sour_file_name) + 2);
    sprintf(dest_path, "%s/%s", sour_id, sour_file_name);
    for(int i = strlen(sour_id) + 1; i < strlen(dest_path); i++) {
        if(dest_path[i] == '/') {
            dest_path[i] = '?';
        } else if(dest_path[i] == '.') {
            dest_path[i] = '_';
        }
    }
    FILE *dest, *sour;
    if(type) {
        dest = fopen(dest_path, "w");
        sour = fopen(sour_file_name, "r");
    } else {
        dest = fopen(sour_file_name, "w");
        sour = fopen(dest_path, "r");
    }
    int c;
    while((c = fgetc(sour)) != EOF) {
        fputc(c, dest);
    }
    fclose(dest);
    fclose(sour);
    free(dest_path);
}

char *svc_commit(void *helper, char *message) {
    // TODO: Implement
    if(!message) return NULL;
    for(int i = 0; i < HEAD->n_files; i++) {
        FILE *fp = fopen(HEAD->files[i].name, "r");
        if(!fp) {
            svc_rm(helper, HEAD->files[i].name);
        } else if(hash_file(helper, HEAD->files[i].name) != HEAD->files[i].hash) {
            int is_del = 0;
            for(int j = 0; j < n_changes; j++) {
                if(!strcmp(changes[j].file_name, HEAD->files[i].name) && !strcmp(changes[j].operation, "del")) {
                    is_del = 1;
                    break;
                }
            }
            if(is_del) {
                fclose(fp);
                continue;
            }
            changes = (change*)realloc(changes, sizeof(change) * ++n_changes);
            changes[n_changes - 1].file_name = strdup(HEAD->files[i].name);
            changes[n_changes - 1].operation = "mod";
            insert_sort();
        }
        if(fp) fclose(fp);
    }

    for(int i = 0; i < n_changes; i++) {
        if(!strcmp(changes[i].operation, "add")) {
            FILE *fp = fopen(changes[i].file_name, "r");
            if(!fp) {
                free(changes[i].file_name);
                for(int j = i; j < n_changes - 1; j++) {
                    changes[j] = changes[j+1];
                }
                changes = (change*)realloc(changes, sizeof(change) * --n_changes);
                break;
            } else {
                fclose(fp);
            }
        }
    }
    if(!n_changes) return NULL;

    commit *com = (commit*)malloc(sizeof(commit));
    com->message = strdup(message);
    com->branches = NULL;
    com->n_branches = 0;
    com->in_branch = curr_branch;
    com->prev = HEAD;
    com->from = NULL;
    com->changes = changes;
    com->n_changes = n_changes;

    com->files = (file*)malloc(sizeof(file) * HEAD->n_files);
    com->n_files = HEAD->n_files;
    for(int i = 0; i < com->n_files; i++) {
        com->files[i].name = strdup(HEAD->files[i].name);
        com->files[i].hash = hash_file(helper, com->files[i].name);
    }

    for(int i = 0; i < com->n_changes; i++) {
        if(!strcmp(com->changes[i].operation, "add")) {
            com->files = (file*)realloc(com->files, sizeof(file) * ++com->n_files);
            com->files[com->n_files - 1].name = strdup(com->changes[i].file_name);
            com->files[com->n_files - 1].hash = hash_file(helper, com->files[com->n_files - 1].name);
        } else if(!strcmp(com->changes[i].operation, "del")) {
            for(int j = 0; j < com->n_files; j++) {
                if(!strcmp(com->changes[i].file_name, com->files[j].name)) {
                    // free files[j].name
                    free(com->files[j].name);
                    for(int k = j; k < com->n_files - 1; k++) {
                        com->files[k] = com->files[k+1];
                    }
                    com->files = (file*)realloc(com->files, sizeof(file) * --com->n_files);
                    break;
                }
            }
        } else {
            for(int j = 0; j < com->n_files; j++) {
                if(!strcmp(com->files[j].name, com->changes[i].file_name)) {
                    com->files[j].hash = hash_file(helper, com->files[j].name);
                    break;
                }
            }
        }
    }
    get_commit_id(com);
    HEAD = com;

    if(!branches[com->in_branch].begin) {
        branches[com->in_branch].begin = com;
    }
    branches[com->in_branch].head = com;
    changes = NULL;
    n_changes = 0;

    mkdir(com->id, 0777);
    // record all file content
    for(int i = 0; i < com->n_files; i++) {
        encode_and_copy(com->id, com->files[i].name, 1);
    }
    return com->id;
}

void *get_commit(void *helper, char *commit_id) {
    // TODO: Implement
    if(!commit_id) return NULL;

    commit *target = NULL;
    for(int i = 0; i < n_branches; i++) {
        commit *com = branches[i].head;
        while(com->id) {
            if(!strcmp(com->id, commit_id)) {
                target = com;
                break;
            }
            com = com->prev;
        }
        if(target) break;
    }
    if(!target && printing) {
        for(int i = 0; i < n_detached; i++) {
            if(!strcmp(detached_commit[i]->id, commit_id)) {
                target = detached_commit[i];
                break;
            }
        }
    }
    return target;
}

char **get_prev_commits(void *helper, void *com, int *n_prev) {
    // TODO: Implement
    if (!n_prev) return NULL;
    *n_prev = 0;
    if (!com) return NULL;
    if (!((commit*)com)->prev->id) return NULL;

    char **ids = NULL;
    commit *ptr = ((commit*)com)->prev;
    if (ptr->id) {
        ids = (char **)realloc(ids, sizeof(char *) * ++(*n_prev));
        ids[*n_prev - 1] = ptr->id;
    }
    if (((commit*)com)->from) {
        ids = (char **)realloc(ids, sizeof(char *) * ++(*n_prev));
        ids[*n_prev - 1] = ((commit*)com)->from->id;
    }
    return ids;
}

void print_commit(void *helper, char *commit_id) {
    // TODO: Implement
    if(!commit_id) {
        printf("Invalid commit id\n");
        return;
    }
    printing = 1;
    commit *target = get_commit(helper, commit_id);
    if(!target) {
        printf("Invalid commit id\n");
        return;
    }

    printf("%s [%s]: %s\n", target->id, branches[target->in_branch].name, target->message);
    for(int i = 0; i < target->n_changes; i++) {
        if(!strcmp(target->changes[i].operation, "add")) {
            printf("    + %s\n", target->changes[i].file_name);
        } else if(!strcmp(target->changes[i].operation, "del")) {
            printf("    - %s\n", target->changes[i].file_name);
        } else {
            int prev_hash, curr_hash;
            for(int j = 0; j < target->prev->n_files; j++) {
                if(!strcmp(target->prev->files[j].name, target->changes[i].file_name)) {
                    prev_hash = target->prev->files[j].hash;
                }
            }
            for(int j = 0; j < target->n_files; j++) {
                if(!strcmp(target->files[j].name, target->changes[i].file_name)) {
                    curr_hash = target->files[j].hash;
                }
            }
            printf("    / %s [%10d -> %10d]\n", target->changes[i].file_name, prev_hash, curr_hash);
        }
    }
    printf("\n    Tracked files (%d):\n", target->n_files);
    for(int i = 0; i < target->n_files; i++) {
        printf("    [%10d] %s\n", target->files[i].hash, target->files[i].name);
    }
    printing = 0;
}

int svc_branch(void *helper, char *branch_name) {
    // TODO: Implement
    if(!branch_name) return -1;
    for(int i = 0; i < strlen(branch_name); i++) {
        char c = branch_name[i];
        if(('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || ('0' <= c && c <= '9') || c == '_' || c == '-' || c == '/') {
            continue;
        } else {
            return -1;
        }
    }
    for(int i = 0; i < n_branches; i++) {
        if(!strcmp(branches[i].name, branch_name)) {
            return -2;
        }
    }
    if(n_changes) return -3;

    branches = (branch*)realloc(branches, sizeof(branch) * ++n_branches);
    branches[n_branches - 1].name = strdup(branch_name);
    branches[n_branches - 1].begin = NULL;
    branches[n_branches - 1].head = HEAD;
    return 0;
}

int svc_checkout(void *helper, char *branch_name) {
    // TODO: Implement
    if(!branch_name) return -1;
    int exist = -1;
    for(int i = 0; i < n_branches; i++) {
        if(!strcmp(branches[i].name, branch_name)) {
            exist = i;
            break;
        }
    }
    if(exist == -1) return -1;
    for(int i = 0; i < HEAD->n_files; i++) {
        if(hash_file(helper, HEAD->files[i].name) == -2) {
            return -2;
        }
    }
    if(n_changes) return -2;

    HEAD = branches[exist].head;
    if(curr_branch != exist) {
        for(int i = 0; i < HEAD->n_files; i++) {
            if(hash_file(helper, HEAD->files[i].name) == HEAD->files[i].hash) {
                continue;
            }
            encode_and_copy(HEAD->id, HEAD->files[i].name, 0);
        }
    }
    curr_branch = exist;
    return 0;
}

char **list_branches(void *helper, int *num_branches) {
    // TODO: Implement
    if(!num_branches) return NULL;

    *num_branches = n_branches;
    char **ret = (char**)malloc(sizeof(char*) * n_branches);
    for(int i = 0; i < n_branches; i++) {
        ret[i] = branches[i].name;
        printf("%s\n", branches[i].name);
    }
    return ret;
}

int svc_add(void *helper, char *file_name) {
    // TODO: Implement
    if(!file_name) return -1;
    for(int i = 0; i < n_changes; i++) {
        if(!strcmp(changes[i].file_name, file_name)) {
            if(!strcmp(changes[i].operation, "add")) {
                return -2;
            } else {
                // free changes -> file_name
                free(changes[i].file_name);
                for(int j = i; j < n_changes - 1; j++) {
                    changes[j] = changes[j+1];
                }
                changes = (change*)realloc(changes, sizeof(change) * --n_changes);
                return 0;
            }
        }
    }
    for(int i = 0; i < HEAD->n_files; i++) {
        if(!strcmp(HEAD->files[i].name, file_name) && HEAD->files[i].hash == hash_file(helper, file_name)) {
            return -2;
        }
    }
    FILE *fp = fopen(file_name, "r");
    if(!fp) {
        return -3;
    }
    fclose(fp);

    changes = (change*)realloc(changes, sizeof(change) * ++n_changes);
    changes[n_changes - 1].file_name = strdup(file_name);
    changes[n_changes - 1].operation = "add";
    insert_sort();
    return hash_file(helper, file_name);
}

int svc_rm(void *helper, char *file_name) {
    // TODO: Implement
    if(!file_name) return -1;
    int in_changes = -1, in_files = -1;
    for(int i = 0; i < HEAD->n_files; i++) {
        if(!strcmp(HEAD->files[i].name, file_name)) {
            in_files = HEAD->files[i].hash;
            break;
        }
    }
    if(in_files == -1) {
        for(int i = 0; i < n_changes; i++) {
            if(!strcmp(changes[i].file_name, file_name) && !strcmp(changes[i].operation, "add")) {
                in_changes = i;
                break;
            }
        }
        if(in_changes == -1) {
            return -2;
        } else {
            // free changes[i].file_name
            free(changes[in_changes].file_name);

            for(int i = in_changes; i < n_changes - 1; i++) {
                changes[i] = changes[i+1];
            }
            changes = (change*)realloc(changes, sizeof(change) * --n_changes);
            return hash_file(helper, file_name);
        }
    } else {
        changes = (change*)realloc(changes, sizeof(change) * ++n_changes);
        changes[n_changes - 1].file_name = strdup(file_name);
        changes[n_changes - 1].operation = "del";
        insert_sort();
        return in_files;
    }
    return 0;
}

void detach_commit(commit *ptr, commit *target) {
    while(ptr != target) {
        detached_commit = (commit**)realloc(detached_commit, sizeof(commit*) * ++n_detached);
        detached_commit[n_detached - 1] = ptr;
        ptr = ptr->prev;
    }
}

int svc_reset(void *helper, char *commit_id) {
    // TODO: Implement
    // HEAD point to commit with commit_id
    if(!commit_id) return -1;
    commit *target = NULL;
    commit *ptr = HEAD;
    while(ptr) {
        if(!ptr->id) break;
        if(!strcmp(ptr->id, commit_id)) {
            target = ptr;
            break;
        }
        ptr = ptr->prev;
    }
    if(!target) {
        return -2;
    }

    HEAD = target;
    // detach commits unreachable
    if(target->in_branch != curr_branch) {
        detach_commit(branches[curr_branch].head, branches[curr_branch].begin->prev);
        branches[curr_branch].head = target;
        branches[curr_branch].begin = NULL;
    }
    detach_commit(branches[target->in_branch].head, target);
    branches[target->in_branch].head = target;

    // discarding all changes
    for(int i = 0; i < n_changes; i++) {
        free(changes[i].file_name);
    }
    n_changes = 0;
    free(changes);
    changes = NULL;

    // restore files content
    for(int i = 0; i < HEAD->n_files; i++) {
        encode_and_copy(HEAD->id, HEAD->files[i].name, 0);
    }
    return 0;
}

char *svc_merge(void *helper, char *branch_name, struct resolution *resolutions, int n_resolutions) {
    // TODO: Implement
    if(!branch_name) {
        printf("Invalid branch name\n");
        return NULL;
    }
    int exist = -1;
    for(int i = 0; i < n_branches; i++) {
        if(!strcmp(branches[i].name, branch_name)) {
            exist = i;
            break;
        }
    }
    if(exist == -1) {
        printf("Branch not found\n");
        return NULL;
    }
    if(exist == curr_branch) {
        printf("Cannot merge a branch with itself\n");
        return NULL;
    }
    if(n_changes) {
        printf("Changes must be committed\n");
        return NULL;
    }

    for(int i = 0; i < HEAD->n_files; i++) {
        FILE *fp = fopen(HEAD->files[i].name, "r");
        if(fp) {
            fclose(fp);
            continue;
        }
        printf("Changes must be committed\n");
        return NULL;
    }
    commit *target = branches[exist].head;
    for(int i = 0; i < target->n_files; i++) {
        int found = 0;
        for(int j = 0; j < HEAD->n_files; j++) {
            if(!strcmp(HEAD->files[j].name, target->files[i].name)) {
                found = 1;
                break;
            }
        }
        if(!found) {
            if(hash_file(helper, target->files[i].name) == target->files[i].hash) {
                svc_add(helper, target->files[i].name);
                continue;
            }
            char *file_name = (char*)malloc(strlen(target->id) + strlen(target->files[i].name) + 2);
            sprintf(file_name, "%s/%s", target->id, target->files[i].name);
            for(int j = strlen(target->id) + 1; j < strlen(file_name); j++) {
                if(file_name[j] == '/') {
                    file_name[j] = '?';
                } else if(file_name[j] == '.') {
                    file_name[j] = '_';
                }
            }
            FILE *sour = fopen(file_name, "r");
            if(!sour) {
                printf("%s cannot be found\n", file_name);
                free(file_name);
                return NULL;
            }
            FILE *dest = fopen(target->files[i].name, "w");
            int c;
            while((c = fgetc(sour)) != EOF) {  // unknown address, zero page
                fputc(c, dest);
            }
            fclose(sour);
            fclose(dest);
            svc_add(helper, target->files[i].name);
            free(file_name);
        }
    }
    for(int i = 0; i < n_resolutions; i++) {
        FILE *sour = fopen(resolutions[i].resolved_file, "r");
        if(!sour) {
            svc_rm(helper, resolutions[i].file_name);
            continue;
        }
        FILE *dest = fopen(resolutions[i].file_name, "w");
        int c;
        while((c = fgetc(sour)) != EOF) {
            fputc(c, dest);
        }
        fclose(dest);
        fclose(sour);
    }
    char *message = (char*)malloc(strlen("Merged branch ") + strlen(branches[target->in_branch].name) + 1);
    sprintf(message, "Merged branch %s", branches[target->in_branch].name);
    char *id = svc_commit(helper, message);
    void *com = get_commit(helper, id);
    ((commit*)com)->from = branches[exist].head;
    free(message);
    puts("Merge successful");
    return id;
}

int main(void) {
    const int input_length = 50;
    char input[input_length];
    printf("Welcome to use Simple Version Control System (VCS)\n");
    while(1) {
        printf("\n");
        fgets(input, input_length, stdin);
        if(!strcmp(input, "quit\n")) break;
        else if(!strcmp(input, "svc init\n")) {
            printf("svc is initialized successfully!\n");
            svc_init();
        }
        // ...
        else {
            break;
        }
    }
    return 0;
}