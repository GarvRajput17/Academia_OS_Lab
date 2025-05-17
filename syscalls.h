#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

// Function prototypes for file locking and system call wrappers
int lock_file(int fd, int type);
int unlock_file(int fd);
ssize_t safe_read(int fd, void *buf, size_t count);
ssize_t safe_write(int fd, const void *buf, size_t count);

// User management
int add_user(const User *user);
int update_user(const User *user);
int find_user_by_name(const char *name, User *user_out);
int find_user_by_id(int id, User *user_out);

// Course management
int add_course(const Course *course);
int update_course(const Course *course);
int find_course_by_id(int id, Course *course_out);
int find_course_by_name(const char *name, Course *course_out);

// Enrollment management
int enroll_student(int student_id, int course_id);
int unenroll_student(int student_id, int course_id);
int is_student_enrolled(int student_id, int course_id);
int list_enrolled_courses(int student_id, Course *courses, int max_courses);

void log_action(const char *fmt, ...);

#endif // SYSCALLS_H 