#include "common.h"
#include "syscalls.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Faculty client menu and actions for Academia Portal
void send_course_request(int sock, int msg_type, Course *course) {
    write(sock, &msg_type, sizeof(int));
    CourseRequest req = {0};
    req.course = *course;
    write(sock, &req, sizeof(CourseRequest));
    // Read lock acquired message or lock error
    GenericResponse resp;
    read(sock, &resp, sizeof(GenericResponse));
    if (resp.success == 0 && strstr(resp.message, "locked by another operation") != NULL) {
        printf("%s\n", resp.message);
        return;
    }
    if (resp.success == 1 && strcmp(resp.message, "fILE LOCK on course file acquired") == 0) {
        // Optionally print this, or just proceed silently
    }
    // Read final result
    read(sock, &resp, sizeof(GenericResponse));
    printf("%s\n", resp.message);
    if (resp.success == 2) { // Server is prompting for confirmation
        printf("%s ", resp.message);
        char confirm = getchar();
        getchar(); // consume newline
        write(sock, &confirm, sizeof(char));
        read(sock, &resp, sizeof(GenericResponse));
        printf("%s\n", resp.message);
    }
}

// Helper to begin add course lock, returns 1 if lock acquired, 0 otherwise
int begin_add_course_lock(int sock) {
    int msg_type = MSG_ADD_COURSE_BEGIN;
    write(sock, &msg_type, sizeof(int));
    GenericResponse resp;
    read(sock, &resp, sizeof(GenericResponse));
    if (!resp.success) {
        printf("%s\n", resp.message);
        return 0;
    }
    return 1;
}

// Helper to end add course lock
void end_add_course_lock(int sock) {
    int msg_type = MSG_ADD_COURSE_END;
    write(sock, &msg_type, sizeof(int));
    GenericResponse resp;
    read(sock, &resp, sizeof(GenericResponse));
}

// Prompt and send add course request (with lock protocol)
void add_course_prompt(int sock, int faculty_id) {
    Course course = {0};
    printf("Enter Course ID: ");
    scanf("%d", &course.id);
    getchar();
    printf("Enter Course Name: ");
    fgets(course.name, MAX_NAME_LEN, stdin);
    course.name[strcspn(course.name, "\n")] = 0;
    course.faculty_id = faculty_id;
    printf("Enter number of seats: ");
    scanf("%d", &course.seats);
    getchar();
    course.enrolled = 0;
    printf("\nYou entered:\n  ID: %d\n  Name: %s\n  Seats: %d\n", course.id, course.name, course.seats);
    send_course_request(sock, MSG_ADD_COURSE, &course);
}

// Prompt and send remove course request
void remove_course_prompt(int sock, int faculty_id) {
    int course_id;
    printf("Enter Course ID to remove: ");
    scanf("%d", &course_id);
    getchar();
    Course course = {0};
    course.id = course_id;
    course.faculty_id = faculty_id;
    send_course_request(sock, MSG_UPDATE_COURSE, &course); // Server will mark as removed
}

// View all courses and enrollments for this faculty
void view_enrollments(int sock, int faculty_id) {
    write(sock, &(int){MSG_LIST_COURSES}, sizeof(int));
    write(sock, &faculty_id, sizeof(int));
    int count;
    read(sock, &count, sizeof(int));
    if (count == 0) {
        printf("No courses found.\n");
        return;
    }
    printf("Your Courses and Enrollments:\n");
    for (int i = 0; i < count; ++i) {
        Course course;
        read(sock, &course, sizeof(Course));
        printf("Course ID: %d, Name: %s, Enrolled: %d/%d\n", course.id, course.name, course.enrolled, course.seats);
    }
}

// View students enrolled in a specific course
void view_students_in_course(int sock, int faculty_id) {
    int course_id;
    printf("Enter Course ID to view enrolled students: ");
    scanf("%d", &course_id);
    getchar();
    write(sock, &(int){MSG_LIST_STUDENTS_IN_COURSE}, sizeof(int));
    write(sock, &course_id, sizeof(int));
    int count;
    read(sock, &count, sizeof(int));
    if (count == 0) {
        printf("No students enrolled or course not found.\n");
        return;
    }
    printf("Enrolled Students (ID, Name):\n");
    for (int i = 0; i < count; ++i) {
        User user;
        read(sock, &user, sizeof(User));
        printf("%d, %s\n", user.id, user.name);
    }
}

// Change faculty password
void change_password_faculty(int sock, int faculty_id) {
    write(sock, &(int){MSG_CHANGE_PASSWORD}, sizeof(int));
    UserRequest req = {0};
    req.user.id = faculty_id;
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

// Prompt and send update course details request
void update_course_details(int sock, int faculty_id) {
    Course course = {0};
    printf("Enter Course ID to update: ");
    scanf("%d", &course.id);
    getchar();
    course.faculty_id = faculty_id;
    printf("Enter new course name (or press Enter to keep): ");
    char buf[MAX_NAME_LEN];
    fgets(buf, MAX_NAME_LEN, stdin);
    if (buf[0] != '\n') {
        buf[strcspn(buf, "\n")] = 0;
        strncpy(course.name, buf, MAX_NAME_LEN);
    }
    printf("Enter new number of seats (or 0 to keep): ");
    int seats;
    scanf("%d", &seats);
    getchar();
    if (seats > 0) course.seats = seats;
    send_course_request(sock, MSG_UPDATE_COURSE, &course);
}

// Faculty main menu loop
void faculty_menu(int sock, int faculty_id) {
    int choice;
    while (1) {
        printf("\nFaculty Menu:\n");
        printf("1. Add new Course\n");
        printf("2. Remove offered Course\n");
        printf("3. Update Course Details\n");
        printf("4. View enrollments in Courses\n");
        printf("5. View students in a Course\n");
        printf("6. Password Change\n");
        printf("7. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        getchar();
        switch (choice) {
            case 1:
                add_course_prompt(sock, faculty_id);
                break;
            case 2:
                remove_course_prompt(sock, faculty_id);
                break;
            case 3:
                update_course_details(sock, faculty_id);
                break;
            case 4:
                view_enrollments(sock, faculty_id);
                break;
            case 5:
                view_students_in_course(sock, faculty_id);
                break;
            case 6:
                change_password_faculty(sock, faculty_id);
                break;
            case 7:
                return;
            default:
                printf("Invalid choice.\n");
        }
    }
} 