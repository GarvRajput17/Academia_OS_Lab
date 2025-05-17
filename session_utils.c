#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "common.h"

#define SESSION_FILE "active_sessions.dat"

int is_user_logged_in(int user_id) {
    int fd = open(SESSION_FILE, O_RDONLY | O_CREAT, 0644);
    if (fd < 0) return 0;
    int id;
    while (read(fd, &id, sizeof(int)) == sizeof(int)) {
        if (id == user_id) {
            close(fd);
            return 1;
        }
    }
    close(fd);
    return 0;
}

void add_active_session(int user_id) {
    int fd = open(SESSION_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    write(fd, &user_id, sizeof(int));
    close(fd);
}

void remove_active_session(int user_id) {
    int fd = open(SESSION_FILE, O_RDWR);
    if (fd < 0) return;
    int ids[MAX_USERS], count = 0, id;
    while (read(fd, &id, sizeof(int)) == sizeof(int)) {
        if (id != user_id) ids[count++] = id;
    }
    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);
    for (int i = 0; i < count; ++i) {
        write(fd, &ids[i], sizeof(int));
    }
    close(fd);
} 