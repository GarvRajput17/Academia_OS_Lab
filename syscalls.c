#include "syscalls.h"
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <stdarg.h>
#include <time.h>

#ifdef SERVER_BUILD
extern sem_t *g_course_sem;
#endif

int lock_file(int fd, int type) {
    struct flock fl;
    fl.l_type = type;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    return fcntl(fd, F_SETLKW, &fl);
}

int unlock_file(int fd) {
    struct flock fl;
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    return fcntl(fd, F_SETLK, &fl);
}

ssize_t safe_read(int fd, void *buf, size_t count) {
    ssize_t ret = read(fd, buf, count);
    if (ret < 0) perror("read");
    return ret;
}

ssize_t safe_write(int fd, const void *buf, size_t count) {
    ssize_t ret = write(fd, buf, count);
    if (ret < 0) perror("write");
    return ret;
}

int add_user(const User *user) {
    // Check for duplicate ID or username (active users only)
    int fd = open(USER_FILE, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return -1;
    lock_file(fd, F_RDLCK);
    User temp;
    while (safe_read(fd, &temp, sizeof(User)) == sizeof(User)) {
        if (temp.active != -1 && (temp.id == user->id || strcmp(temp.name, user->name) == 0)) {
            unlock_file(fd);
            close(fd);
            return -2; // Duplicate
        }
    }
    unlock_file(fd);
    close(fd);
    // Add user
    fd = open(USER_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return -1;
    lock_file(fd, F_WRLCK);
    ssize_t written = safe_write(fd, user, sizeof(User));
    unlock_file(fd);
    close(fd);
    return (written == sizeof(User)) ? 0 : -1;
}

int update_user(const User *user) {
    int fd = open(USER_FILE, O_RDWR);
    if (fd < 0) return -1;
    lock_file(fd, F_WRLCK);
    User temp;
    ssize_t pos = 0;
    while (safe_read(fd, &temp, sizeof(User)) == sizeof(User)) {
        if (temp.id == user->id) {
            lseek(fd, pos, SEEK_SET);
            ssize_t written = safe_write(fd, user, sizeof(User));
            unlock_file(fd);
            close(fd);
            return (written == sizeof(User)) ? 0 : -1;
        }
        pos += sizeof(User);
    }
    unlock_file(fd);
    close(fd);
    return -1;
}

int find_user_by_name(const char *name, User *user_out) {
    int fd = open(USER_FILE, O_RDONLY);
    if (fd < 0) return -1;
    lock_file(fd, F_RDLCK);
    User temp;
    while (safe_read(fd, &temp, sizeof(User)) == sizeof(User)) {
        if (strcmp(temp.name, name) == 0) {
            *user_out = temp;
            unlock_file(fd);
            close(fd);
            return 0;
        }
    }
    unlock_file(fd);
    close(fd);
    return -1;
}

int find_user_by_id(int id, User *user_out) {
    int fd = open(USER_FILE, O_RDONLY);
    if (fd < 0) return -1;
    lock_file(fd, F_RDLCK);
    User temp;
    while (safe_read(fd, &temp, sizeof(User)) == sizeof(User)) {
        if (temp.id == id) {
            *user_out = temp;
            unlock_file(fd);
            close(fd);
            return 0;
        }
    }
    unlock_file(fd);
    close(fd);
    return -1;
}

int add_course(const Course *course) {
    int fd = open(COURSE_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return -1;
    lock_file(fd, F_WRLCK);
    ssize_t written = safe_write(fd, course, sizeof(Course));
    unlock_file(fd);
    close(fd);
    return (written == sizeof(Course)) ? 0 : -1;
}

int update_course(const Course *course) {
    int fd = open(COURSE_FILE, O_RDWR);
    if (fd < 0) return -1;
    lock_file(fd, F_WRLCK);
    Course temp;
    ssize_t pos = 0;
    while (safe_read(fd, &temp, sizeof(Course)) == sizeof(Course)) {
        if (temp.id == course->id) {
            lseek(fd, pos, SEEK_SET);
            ssize_t written = safe_write(fd, course, sizeof(Course));
            unlock_file(fd);
            close(fd);
            return (written == sizeof(Course)) ? 0 : -1;
        }
        pos += sizeof(Course);
    }
    unlock_file(fd);
    close(fd);
    return -1;
}

int find_course_by_id(int id, Course *course_out) {
    int fd = open(COURSE_FILE, O_RDONLY);
    if (fd < 0) return -1;
    lock_file(fd, F_RDLCK);
    Course temp;
    while (safe_read(fd, &temp, sizeof(Course)) == sizeof(Course)) {
        if (temp.id == id) {
            *course_out = temp;
            unlock_file(fd);
            close(fd);
            return 0;
        }
    }
    unlock_file(fd);
    close(fd);
    return -1;
}

int find_course_by_name(const char *name, Course *course_out) {
    int fd = open(COURSE_FILE, O_RDONLY);
    if (fd < 0) return -1;
    lock_file(fd, F_RDLCK);
    Course temp;
    while (safe_read(fd, &temp, sizeof(Course)) == sizeof(Course)) {
        if (strcmp(temp.name, name) == 0) {
            *course_out = temp;
            unlock_file(fd);
            close(fd);
            return 0;
        }
    }
    unlock_file(fd);
    close(fd);
    return -1;
}

int enroll_student(int student_id, int course_id) {
#ifdef SERVER_BUILD
    posix_sem_wait(g_course_sem);
#endif
    // Check if already enrolled
    if (is_student_enrolled(student_id, course_id)) {
#ifdef SERVER_BUILD
        posix_sem_signal(g_course_sem);
#endif
        return -1;
    }
    // Check course seat availability
    Course course;
    if (find_course_by_id(course_id, &course) != 0 || course.enrolled >= course.seats) {
#ifdef SERVER_BUILD
        posix_sem_signal(g_course_sem);
#endif
        return -2;
    }
    // Add enrollment
    int fd = open(ENROLLMENT_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
#ifdef SERVER_BUILD
        posix_sem_signal(g_course_sem);
#endif
        return -1;
    }
    lock_file(fd, F_WRLCK);
    Enrollment enr = {student_id, course_id};
    ssize_t written = safe_write(fd, &enr, sizeof(Enrollment));
    unlock_file(fd);
    close(fd);
    // Update course enrolled count
    course.enrolled++;
    update_course(&course);
#ifdef SERVER_BUILD
    posix_sem_signal(g_course_sem);
#endif
    return (written == sizeof(Enrollment)) ? 0 : -1;
}

int unenroll_student(int student_id, int course_id) {
    int fd = open(ENROLLMENT_FILE, O_RDWR);
    if (fd < 0) return -1;
    lock_file(fd, F_WRLCK);
    Enrollment enr;
    ssize_t pos = 0;
    int found = 0;
    while (safe_read(fd, &enr, sizeof(Enrollment)) == sizeof(Enrollment)) {
        if (enr.student_id == student_id && enr.course_id == course_id) {
            found = 1;
            break;
        }
        pos += sizeof(Enrollment);
    }
    if (!found) {
        unlock_file(fd);
        close(fd);
        return -1;
    }
    // Remove enrollment by marking as -1
    Enrollment blank = {-1, -1};
    lseek(fd, pos, SEEK_SET);
    safe_write(fd, &blank, sizeof(Enrollment));
    unlock_file(fd);
    close(fd);
    // Update course enrolled count
    Course course;
    if (find_course_by_id(course_id, &course) == 0 && course.enrolled > 0) {
        course.enrolled--;
        update_course(&course);
    }
    return 0;
}

int is_student_enrolled(int student_id, int course_id) {
    int fd = open(ENROLLMENT_FILE, O_RDONLY);
    if (fd < 0) return 0;
    lock_file(fd, F_RDLCK);
    Enrollment enr;
    while (safe_read(fd, &enr, sizeof(Enrollment)) == sizeof(Enrollment)) {
        if (enr.student_id == student_id && enr.course_id == course_id) {
            unlock_file(fd);
            close(fd);
            return 1;
        }
    }
    unlock_file(fd);
    close(fd);
    return 0;
}

int list_enrolled_courses(int student_id, Course *courses, int max_courses) {
    int fd = open(ENROLLMENT_FILE, O_RDONLY);
    if (fd < 0) return 0;
    lock_file(fd, F_RDLCK);
    Enrollment enr;
    int count = 0;
    while (safe_read(fd, &enr, sizeof(Enrollment)) == sizeof(Enrollment)) {
        if (enr.student_id == student_id && enr.course_id != -1) {
            if (count < max_courses) {
                find_course_by_id(enr.course_id, &courses[count]);
                count++;
            }
        }
    }
    unlock_file(fd);
    close(fd);
    return count;
}

// POSIX semaphore helpers
sem_t *init_semaphore(const char *name, int initial_value) {
    sem_t *sem = sem_open(name, O_CREAT, 0666, initial_value);
    return sem;
}

void destroy_semaphore(const char *name, sem_t *sem) {
    sem_close(sem);
    sem_unlink(name);
}

void posix_sem_wait(sem_t *sem) {
    sem_wait(sem);
}

void posix_sem_signal(sem_t *sem) {
    sem_post(sem);
}

void log_action(const char *fmt, ...) {
    FILE *f = fopen("actions.log", "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(f, "[%s] ", timebuf);
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fprintf(f, "\n");
    fclose(f);
} 