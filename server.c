#include "socket_utils.h"
#include "syscalls.h"
#include "common.h"
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <semaphore.h>

sem_t *g_course_sem = NULL;
sem_t *course_update_sem = NULL;

// Stub: check credentials (replace with file-based check later)
int check_user(const char *name, const char *password, UserRole *role, int *user_id) {
    // Check admin hardcoded
    if (strcmp(name, "admin") == 0 && strcmp(password, "adminpass") == 0) {
        *role = ROLE_ADMIN;
        *user_id = 1;
        return 1;
    }
    // Check users.dat for all other users
    User user;
    if (find_user_by_name(name, &user) == 0 && user.active == 1 && strcmp(user.password, password) == 0) {
        *role = user.role;
        *user_id = user.id;
        return 1;
    }
    return 0;
}

// Handles admin requests (add/update/delete users, consistency check, etc.)
void handle_admin_requests(int client_fd) {
    int msg_type;
    while (read(client_fd, &msg_type, sizeof(int)) > 0) {
        if (msg_type == MSG_ADD_USER) {
            UserRequest req;
            read(client_fd, &req, sizeof(UserRequest));
            GenericResponse resp = {0};
            int add_result = add_user(&req.user);
            if (add_result == 0) {
                resp.success = 1;
                snprintf(resp.message, MAX_LINE, "User added successfully!");
            } else if (add_result == -2) {
                resp.success = -2;
                snprintf(resp.message, MAX_LINE, "Duplicate user ID or username.");
            } else {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Failed to add user.");
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else if (msg_type == MSG_UPDATE_USER) {
            UserRequest req;
            read(client_fd, &req, sizeof(UserRequest));
            GenericResponse resp = {0};
            User user;
            if (find_user_by_id(req.user.id, &user) == 0) {
                if (req.user.name[0] != '\0') strncpy(user.name, req.user.name, MAX_NAME_LEN);
                if (req.user.password[0] != '\0') strncpy(user.password, req.user.password, MAX_PASS_LEN);
                if (req.user.active == 0 || req.user.active == 1) user.active = req.user.active;
                update_user(&user);
                resp.success = 1;
                snprintf(resp.message, MAX_LINE, "User updated.");
            } else {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "User not found.");
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else if (msg_type == MSG_DELETE_USER) {
            int id;
            read(client_fd, &id, sizeof(int));
            GenericResponse resp = {0};
            User user;
            if (find_user_by_id(id, &user) == 0 && user.active != -1) {
                user.active = -1;
                update_user(&user);
                resp.success = 1;
                snprintf(resp.message, MAX_LINE, "User deleted.");
                log_action("Admin deleted user %d (%s)", user.id, user.name);
            } else {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "User not found or already deleted.");
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else if (msg_type == MSG_CONSISTENCY_CHECK) {
            GenericResponse resp = {0};
            int fixed = 0, total = 0;
            // Scan enrollments and remove invalid ones
            int fd = open(ENROLLMENT_FILE, O_RDWR);
            if (fd >= 0) {
                lock_file(fd, F_WRLCK);
                Enrollment enr;
                ssize_t pos = 0;
                while (safe_read(fd, &enr, sizeof(Enrollment)) == sizeof(Enrollment)) {
                    total++;
                    // Check student
                    User user;
                    int user_ok = (find_user_by_id(enr.student_id, &user) == 0 && user.active == 1 && user.role == ROLE_STUDENT);
                    // Check course
                    Course course;
                    int course_ok = (find_course_by_id(enr.course_id, &course) == 0 && course.seats > 0);
                    if (!user_ok || !course_ok) {
                        Enrollment blank = {-1, -1};
                        lseek(fd, pos, SEEK_SET);
                        safe_write(fd, &blank, sizeof(Enrollment));
                        fixed++;
                    }
                    pos += sizeof(Enrollment);
                }
                unlock_file(fd);
                close(fd);
            }
            snprintf(resp.message, MAX_LINE, "Consistency check done. %d/%d invalid enrollments removed.", fixed, total);
            resp.success = 1;
            write(client_fd, &resp, sizeof(GenericResponse));
            log_action("Admin ran consistency check: %d/%d invalid enrollments removed", fixed, total);
        } else if (msg_type == MSG_PURGE_DELETED_USERS) {
            GenericResponse resp = {0};
            int count = 0, kept = 0;
            FILE *fin = fopen(USER_FILE, "rb");
            FILE *fout = fopen("users.tmp", "wb");
            if (fin && fout) {
                User user;
                while (fread(&user, sizeof(User), 1, fin) == 1) {
                    count++;
                    if (user.active != -1) {
                        fwrite(&user, sizeof(User), 1, fout);
                        kept++;
                    }
                }
                fclose(fin);
                fclose(fout);
                remove(USER_FILE);
                rename("users.tmp", USER_FILE);
                snprintf(resp.message, MAX_LINE, "Purge complete. %d deleted users removed, %d kept.", count - kept, kept);
                resp.success = 1;
                log_action("Admin purged deleted users: %d removed, %d kept", count - kept, kept);
            } else {
                snprintf(resp.message, MAX_LINE, "Failed to purge deleted users.");
                resp.success = 0;
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else {
            break;
        }
    }
}

// Handles student requests (enroll, drop, view courses, change password)
void handle_student_requests(int client_fd, int student_id) {
    int msg_type;
    while (read(client_fd, &msg_type, sizeof(int)) > 0) {
        if (msg_type == MSG_ENROLL_COURSE) {
            EnrollRequest req;
            read(client_fd, &req, sizeof(EnrollRequest));
            GenericResponse resp = {0};
            int res = enroll_student(req.student_id, req.course_id);
            if (res == 0) {
                resp.success = 1;
                snprintf(resp.message, MAX_LINE, "Enrolled successfully!");
            } else if (res == -1) {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Already enrolled or error.");
            } else if (res == -2) {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Course full or not found.");
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else if (msg_type == MSG_UNENROLL_COURSE) {
            EnrollRequest req;
            read(client_fd, &req, sizeof(EnrollRequest));
            GenericResponse resp = {0};
            if (unenroll_student(req.student_id, req.course_id) == 0) {
                resp.success = 1;
                snprintf(resp.message, MAX_LINE, "Unenrolled successfully!");
            } else {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Not enrolled in this course.");
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else if (msg_type == MSG_LIST_ENROLLMENTS) {
            int sid;
            read(client_fd, &sid, sizeof(int));
            Course courses[MAX_COURSES];
            int count = list_enrolled_courses(sid, courses, MAX_COURSES);
            write(client_fd, &count, sizeof(int));
            for (int i = 0; i < count; ++i) {
                write(client_fd, &courses[i], sizeof(Course));
            }
        } else if (msg_type == MSG_LIST_COURSES) {
            int fid;
            read(client_fd, &fid, sizeof(int)); // should be -1 for all courses
            Course courses[MAX_COURSES];
            int count = 0;
            int fd = open(COURSE_FILE, O_RDONLY);
            if (fd >= 0) {
                lock_file(fd, F_RDLCK);
                Course course;
                while (safe_read(fd, &course, sizeof(Course)) == sizeof(Course)) {
                    if (course.seats > 0 && course.id > 0 && course.name[0] != '\0') {
                        courses[count++] = course;
                    }
                }
                unlock_file(fd);
                close(fd);
            }
            write(client_fd, &count, sizeof(int));
            for (int i = 0; i < count; ++i) {
                write(client_fd, &courses[i], sizeof(Course));
            }
        } else if (msg_type == MSG_CHANGE_PASSWORD) {
            UserRequest req;
            read(client_fd, &req, sizeof(UserRequest));
            GenericResponse resp = {0};
            User user;
            if (find_user_by_id(req.user.id, &user) == 0) {
                if (req.user.password[0] != '\0') strncpy(user.password, req.user.password, MAX_PASS_LEN);
                update_user(&user);
                resp.success = 1;
                snprintf(resp.message, MAX_LINE, "Password changed.");
            } else {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "User not found.");
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else {
            break;
        }
    }
}

// Handles faculty requests (add/update/remove courses, view enrollments, change password)
void handle_faculty_requests(int client_fd, int faculty_id) {
    int msg_type;
    while (read(client_fd, &msg_type, sizeof(int)) > 0) {
        if (msg_type == MSG_ADD_COURSE) {
            CourseRequest req;
            read(client_fd, &req, sizeof(CourseRequest));
            GenericResponse resp = {0};
            if (add_course(&req.course) == 0) {
                resp.success = 1;
                snprintf(resp.message, MAX_LINE, "Course added successfully!");
            } else {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Failed to add course.");
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else if (msg_type == MSG_UPDATE_COURSE) {
            CourseRequest req;
            read(client_fd, &req, sizeof(CourseRequest));
            GenericResponse resp = {0};
            Course course;
            if (find_course_by_id(req.course.id, &course) == 0 && course.faculty_id == faculty_id) {
                if (req.course.name[0] != '\0') strncpy(course.name, req.course.name, MAX_NAME_LEN);
                if (req.course.seats > 0) course.seats = req.course.seats;
                else if (req.course.seats == 0 && req.course.name[0] == '\0') course.seats = 0;
                update_course(&course);
                resp.success = 1;
                if (req.course.seats == 0 && req.course.name[0] == '\0')
                    snprintf(resp.message, MAX_LINE, "Course removed (marked as unavailable).");
                else
                    snprintf(resp.message, MAX_LINE, "Course updated.");
            } else {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Course not found or not owned by you.");
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else if (msg_type == MSG_LIST_COURSES) {
            int fid;
            read(client_fd, &fid, sizeof(int));
            Course courses[MAX_COURSES];
            int count = 0;
            if (sem_trywait(course_update_sem) != 0) {
                // Could not acquire lock, send error
                count = -1;
                write(client_fd, &count, sizeof(int));
            } else {
                int fd = open(COURSE_FILE, O_RDONLY);
                if (fd >= 0) {
                    lock_file(fd, F_RDLCK);
                    Course course;
                    int entry = 0;
                    while (safe_read(fd, &course, sizeof(Course)) == sizeof(Course)) {
                        if ((fid == -1 && course.seats > 0 && course.id > 0 && course.name[0] != '\0') ||
                            (course.faculty_id == fid && course.seats > 0 && course.id > 0 && course.name[0] != '\0')) {
                            courses[count++] = course;
                        }
                        entry++;
                    }
                    unlock_file(fd);
                    close(fd);
                    fprintf(stderr, "[SERVER] Read %d entries from courses.dat, sending %d valid courses to client\n", entry, count);
                } else {
                    fprintf(stderr, "[SERVER] Could not open courses.dat for reading\n");
                }
                sem_post(course_update_sem);
                if (write(client_fd, &count, sizeof(int)) != sizeof(int)) {
                    fprintf(stderr, "[SERVER] Error writing course count to client\n");
                }
                for (int i = 0; i < count; ++i) {
                    if (write(client_fd, &courses[i], sizeof(Course)) != sizeof(Course)) {
                        fprintf(stderr, "[SERVER] Error writing course data to client (i=%d)\n", i);
                    }
                }
            }
        } else if (msg_type == MSG_LIST_STUDENTS_IN_COURSE) {
            int course_id;
            read(client_fd, &course_id, sizeof(int));
            // Check if faculty owns the course
            Course course;
            int count = 0;
            User students[MAX_USERS];
            if (find_course_by_id(course_id, &course) == 0 && course.faculty_id == faculty_id && course.seats > 0) {
                // Find all enrollments for this course
                int fd = open(ENROLLMENT_FILE, O_RDONLY);
                if (fd >= 0) {
                    lock_file(fd, F_RDLCK);
                    Enrollment enr;
                    while (safe_read(fd, &enr, sizeof(Enrollment)) == sizeof(Enrollment)) {
                        if (enr.course_id == course_id && enr.student_id != -1) {
                            User user;
                            if (find_user_by_id(enr.student_id, &user) == 0 && user.active == 1 && user.role == ROLE_STUDENT) {
                                students[count++] = user;
                            }
                        }
                    }
                    unlock_file(fd);
                    close(fd);
                }
            }
            write(client_fd, &count, sizeof(int));
            for (int i = 0; i < count; ++i) {
                write(client_fd, &students[i], sizeof(User));
            }
            log_action("Faculty %d viewed students in course %d (count=%d)", faculty_id, course_id, count);
        } else if (msg_type == MSG_CHANGE_PASSWORD) {
            UserRequest req;
            read(client_fd, &req, sizeof(UserRequest));
            GenericResponse resp = {0};
            User user;
            if (find_user_by_id(req.user.id, &user) == 0) {
                if (req.user.password[0] != '\0') strncpy(user.password, req.user.password, MAX_PASS_LEN);
                update_user(&user);
                resp.success = 1;
                snprintf(resp.message, MAX_LINE, "Password changed.");
            } else {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "User not found.");
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else {
            break;
        }
    }
}

// Server entry point and client handler for Academia Portal
void handle_client(int client_fd) {
    int msg_type;
    read(client_fd, &msg_type, sizeof(int));
    if (msg_type == MSG_LOGIN_REQUEST) {
        LoginRequest req;
        read(client_fd, &req, sizeof(LoginRequest));
        LoginResponse resp = {0};
        UserRole role;
        int user_id;
        if (check_user(req.name, req.password, &role, &user_id)) {
            resp.success = 1;
            resp.role = role;
            resp.user_id = user_id;
            snprintf(resp.message, MAX_LINE, "Welcome!");
            write(client_fd, &resp, sizeof(LoginResponse));
            if (role == ROLE_ADMIN) {
                handle_admin_requests(client_fd);
            } else if (role == ROLE_STUDENT) {
                handle_student_requests(client_fd, user_id);
            } else if (role == ROLE_FACULTY) {
                handle_faculty_requests(client_fd, user_id);
            }
        } else {
            resp.success = 0;
            snprintf(resp.message, MAX_LINE, "Invalid credentials");
            write(client_fd, &resp, sizeof(LoginResponse));
        }
    }
    close(client_fd);
}

// Main server loop: accepts clients and forks child processes
int main() {
    g_course_sem = init_semaphore("/course_sem", 1);
    course_update_sem = sem_open("/course_update_sem", O_CREAT, 0666, 1);
    int server_fd = create_server_socket();
    printf("Server started. Waiting for clients...\n");
    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        pid_t pid = fork();
        if (pid == 0) {
            close(server_fd);
            handle_client(client_fd);
            _exit(0);
        } else {
            close(client_fd);
            // Clean up zombies
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }
    close(server_fd);
    destroy_semaphore("/course_sem", g_course_sem);
    sem_close(course_update_sem);
    sem_unlink("/course_update_sem");
    return 0;
} 