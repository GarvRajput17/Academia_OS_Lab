#include "common.h"
#include "syscalls.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef STUDENT_H
#define STUDENT_H
void student_menu(int sock, int student_id);
#endif

// Student client menu and actions for Academia Portal
void send_enroll_request(int sock, int msg_type, int student_id, int course_id) {
    write(sock, &msg_type, sizeof(int));
    EnrollRequest req = {student_id, course_id};
    write(sock, &req, sizeof(EnrollRequest));
    GenericResponse resp;
    read(sock, &resp, sizeof(GenericResponse));
    printf("%s\n", resp.message);
}

// Prompt and enroll in a course
void enroll_in_course(int sock, int student_id) {
    int course_id;
    printf("Enter Course ID to enroll: ");
    scanf("%d", &course_id);
    getchar();
    send_enroll_request(sock, MSG_ENROLL_COURSE, student_id, course_id);
}

// Prompt and unenroll from a course
void unenroll_from_course(int sock, int student_id) {
    int course_id;
    printf("Enter Course ID to unenroll: ");
    scanf("%d", &course_id);
    getchar();
    send_enroll_request(sock, MSG_UNENROLL_COURSE, student_id, course_id);
}

// View all enrolled courses for this student
void view_enrolled_courses(int sock, int student_id) {
    write(sock, &(int){MSG_LIST_ENROLLMENTS}, sizeof(int));
    write(sock, &student_id, sizeof(int));
    int count;
    read(sock, &count, sizeof(int));
    if (count == 0) {
        printf("No enrolled courses.\n");
        return;
    }
    printf("Enrolled Courses:\n");
    for (int i = 0; i < count; ++i) {
        Course course;
        read(sock, &course, sizeof(Course));
        printf("ID: %d, Name: %s, Faculty ID: %d\n", course.id, course.name, course.faculty_id);
    }
}

// Change student password
void change_password(int sock, int student_id) {
    write(sock, &(int){MSG_CHANGE_PASSWORD}, sizeof(int));
    UserRequest req = {0};
    req.user.id = student_id;
    printf("Enter new password: ");
    char buf[MAX_PASS_LEN];
    fgets(buf, MAX_PASS_LEN, stdin);
    buf[strcspn(buf, "\n")] = 0;
    strncpy(req.user.password, buf, MAX_PASS_LEN);
    write(sock, &req, sizeof(UserRequest));
    GenericResponse resp;
    read(sock, &resp, sizeof(GenericResponse));
    printf("%s\n", resp.message);
}

// View all available courses
void view_all_courses(int sock) {
    write(sock, &(int){MSG_LIST_COURSES}, sizeof(int));
    int dummy = -1;
    write(sock, &dummy, sizeof(int)); // -1 means all courses

    int count;
    if (read(sock, &count, sizeof(int)) != sizeof(int)) {
        perror("Error reading course count");
        return;
    }

    if (count <= 0) {
        printf("No available courses.\n");
        return;
    }

    printf("Available Courses:\n");
    for (int i = 0; i < count; ++i) {
        Course course;
        if (read(sock, &course, sizeof(Course)) != sizeof(Course)) {
            perror("Error reading course data");
            return;
        }
        printf("ID: %d, Name: %s, Faculty ID: %d, Seats: %d, Enrolled: %d\n",
               course.id, course.name, course.faculty_id, course.seats, course.enrolled);
    }
}

// Prompt and drop a course
void drop_course(int sock, int student_id) {
    int course_id;
    printf("Enter Course ID to drop: ");
    scanf("%d", &course_id);
    getchar();
    send_enroll_request(sock, MSG_UNENROLL_COURSE, student_id, course_id);
}

// Student main menu loop
void student_menu(int sock, int student_id) {
    int choice;
    while (1) {
        printf("\nStudent Menu:\n");
        printf("1. Enroll in Course\n");
        printf("2. Drop (Unenroll) from Course\n");
        printf("3. View Enrolled Courses\n");
        printf("4. View All Available Courses\n");
        printf("5. Change Password\n");
        printf("6. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        getchar();
        switch (choice) {
            case 1:
                enroll_in_course(sock, student_id);
                break;
            case 2:
                drop_course(sock, student_id);
                break;
            case 3:
                view_enrolled_courses(sock, student_id);
                break;
            case 4:
                view_all_courses(sock);
                break;
            case 5:
                change_password(sock, student_id);
                break;
            case 6:
                return;
            default:
                printf("Invalid choice.\n");
        }
    }
} 