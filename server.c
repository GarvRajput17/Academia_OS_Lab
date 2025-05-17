#include "socket_utils.h"
#include "syscalls.h"
#include "common.h"
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include "session_utils.h"

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
            read(client_fd, &fid, sizeof(int));
            Course courses[MAX_COURSES];
            int count = 0;
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
            if (write(client_fd, &count, sizeof(int)) != sizeof(int)) {
                fprintf(stderr, "[SERVER] Error writing course count to client\n");
            }
            for (int i = 0; i < count; ++i) {
                if (write(client_fd, &courses[i], sizeof(Course)) != sizeof(Course)) {
                    fprintf(stderr, "[SERVER] Error writing course data to client (i=%d)\n", i);
                }
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
    printf("[SERVER DEBUG] handle_faculty_requests CALLED for faculty_id=%d\n", faculty_id);
    fflush(stdout);
    int msg_type;
    while (read(client_fd, &msg_type, sizeof(int)) > 0) {
        if (msg_type == MSG_ADD_COURSE) {
            printf("[SERVER DEBUG] Entered MSG_ADD_COURSE block\n");
            fflush(stdout);
            CourseRequest req;
            read(client_fd, &req, sizeof(CourseRequest));
            printf("[SERVER DEBUG] Read CourseRequest from client\n");
            fflush(stdout);
            int fd = open(COURSE_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                printf("[SERVER DEBUG] Failed to open course file\n");
                fflush(stdout);
                GenericResponse resp = {0};
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Failed to open course file.");
                write(client_fd, &resp, sizeof(GenericResponse));
                return;
            }
            printf("[SERVER DEBUG] Opened course file, about to lock\n");
            fflush(stdout);
            struct flock fl;
            fl.l_type = F_WRLCK;
            fl.l_whence = SEEK_SET;
            fl.l_start = 0;
            fl.l_len = 0;
            int lock_result = fcntl(fd, F_SETLK, &fl); // Non-blocking
            if (lock_result == -1) {
                printf("[SERVER] Faculty %d cannot access because course file is locked\n", faculty_id);
                fflush(stdout);
                GenericResponse resp = {0};
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Course file is currently locked by another operation. Please try again later.");
                write(client_fd, &resp, sizeof(GenericResponse));
                close(fd);
                continue;
            }
            GenericResponse lock_resp = {1, "fILE LOCK on course file acquired"};
            write(client_fd, &lock_resp, sizeof(GenericResponse));
            sleep(8); // Simulate long-running operation for demo
            ssize_t written = safe_write(fd, &req.course, sizeof(Course));
            unlock_file(fd);
            close(fd);
            GenericResponse resp = {0};
            if (written == sizeof(Course)) {
                resp.success = 1;
                printf("[SERVER] Course added successfully!\n");
                fflush(stdout);
                snprintf(resp.message, MAX_LINE, "Course added successfully!");
            } else {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Failed to add course.");
            }
            write(client_fd, &resp, sizeof(GenericResponse));
        } else if (msg_type == MSG_UPDATE_COURSE) {
            printf("[SERVER DEBUG] Received MSG_UPDATE_COURSE request\n");
            fflush(stdout);
            CourseRequest req;
            read(client_fd, &req, sizeof(CourseRequest));
            printf("[SERVER DEBUG] Read CourseRequest from client\n");
            fflush(stdout);
            int fd = open(COURSE_FILE, O_RDWR);
            if (fd < 0) {
                printf("[SERVER DEBUG] Failed to open course file\n");
                fflush(stdout);
                GenericResponse resp = {0};
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Failed to open course file.");
                write(client_fd, &resp, sizeof(GenericResponse));
                return;
            }
            printf("[SERVER DEBUG] Opened course file, about to lock\n");
            fflush(stdout);
            int lock_result = lock_file(fd, F_WRLCK);
            printf("[SERVER DEBUG] lock_file returned %d\n", lock_result);
            fflush(stdout);
            if (lock_result == 0) {
                printf("[SERVER] fILE LOCK on course file acquired for UPDATE_COURSE (lock_file returned 0)\n");
                fflush(stdout);
            } else {
                printf("[SERVER] lock_file failed for UPDATE_COURSE, return value: %d\n", lock_result);
                fflush(stdout);
            }
            GenericResponse lock_resp = {1, "fILE LOCK on course file acquired"};
            write(client_fd, &lock_resp, sizeof(GenericResponse));
            printf("[SERVER DEBUG] Sent lock acquired message to client\n");
            fflush(stdout);
            GenericResponse resp = {0};
            Course course;
            if (find_course_by_id(req.course.id, &course) == 0 && course.faculty_id == faculty_id) {
                if (req.course.name[0] != '\0') strncpy(course.name, req.course.name, MAX_NAME_LEN);
                if (req.course.seats > 0) course.seats = req.course.seats;
                else if (req.course.seats == 0 && req.course.name[0] == '\0') course.seats = 0;
                // Write the updated course back to the file
                Course temp;
                ssize_t pos = 0;
                lseek(fd, 0, SEEK_SET);
                while (safe_read(fd, &temp, sizeof(Course)) == sizeof(Course)) {
                    if (temp.id == course.id) {
                        lseek(fd, pos, SEEK_SET);
                        safe_write(fd, &course, sizeof(Course));
                        break;
                    }
                    pos += sizeof(Course);
                }
                resp.success = 1;
                if (req.course.seats == 0 && req.course.name[0] == '\0')
                    snprintf(resp.message, MAX_LINE, "Course removed (marked as unavailable).");
                else
                    snprintf(resp.message, MAX_LINE, "Course updated.");
            } else {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "Course not found or not owned by you.");
            }
            unlock_file(fd);
            printf("[SERVER DEBUG] Unlocked file\n");
            fflush(stdout);
            close(fd);
            printf("[SERVER DEBUG] Closed file\n");
            fflush(stdout);
            write(client_fd, &resp, sizeof(GenericResponse));
            printf("[SERVER DEBUG] Sent final response to client\n");
            fflush(stdout);
        } else if (msg_type == MSG_LIST_COURSES) {
            int fid;
            read(client_fd, &fid, sizeof(int));
            Course courses[MAX_COURSES];
            int count = 0;
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
            if (write(client_fd, &count, sizeof(int)) != sizeof(int)) {
                fprintf(stderr, "[SERVER] Error writing course count to client\n");
            }
            for (int i = 0; i < count; ++i) {
                if (write(client_fd, &courses[i], sizeof(Course)) != sizeof(Course)) {
                    fprintf(stderr, "[SERVER] Error writing course data to client (i=%d)\n", i);
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
    int user_id = -1;
    UserRole role;
    int logged_in = 0;
    read(client_fd, &msg_type, sizeof(int));
    if (msg_type == MSG_LOGIN_REQUEST) {
        LoginRequest req;
        read(client_fd, &req, sizeof(LoginRequest));
        LoginResponse resp = {0};
        if (check_user(req.name, req.password, &role, &user_id)) {
            // Session management: disallow multiple logins
            if (is_user_logged_in(user_id)) {
                resp.success = 0;
                snprintf(resp.message, MAX_LINE, "User already logged in elsewhere.");
                write(client_fd, &resp, sizeof(LoginResponse));
                close(client_fd);
                return;
            }
            add_active_session(user_id);
            logged_in = 1;
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
    if (logged_in && user_id != -1) {
        remove_active_session(user_id);
    }
    close(client_fd);
}

// Main server loop: accepts clients and forks child processes
int main() {
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
    return 0;
} 