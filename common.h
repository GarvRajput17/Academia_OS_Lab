#ifndef COMMON_H
#define COMMON_H

#define MAX_USERS 100
#define MAX_COURSES 100
#define MAX_NAME_LEN 64
#define MAX_PASS_LEN 64
#define MAX_LINE 256
#define MAX_SEATS 50
#define USER_FILE "users.dat"
#define COURSE_FILE "courses.dat"
#define ENROLLMENT_FILE "enrollments.dat"

// User roles
typedef enum { ROLE_ADMIN, ROLE_STUDENT, ROLE_FACULTY } UserRole;

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    char password[MAX_PASS_LEN];
    int active; // 1 = active, 0 = deactivated
    UserRole role;
} User;

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    int faculty_id;
    int seats;
    int enrolled;
} Course;

typedef struct {
    int student_id;
    int course_id;
} Enrollment;

// Protocol message types
#define MSG_LOGIN_REQUEST 1
#define MSG_LOGIN_RESPONSE 2

// Login request/response
typedef struct {
    char name[MAX_NAME_LEN];
    char password[MAX_PASS_LEN];
} LoginRequest;

typedef struct {
    int success; // 1 = success, 0 = fail
    UserRole role;
    int user_id;
    char message[MAX_LINE];
} LoginResponse;

// Protocol message types for menu actions
#define MSG_ADD_USER 10
#define MSG_UPDATE_USER 11
#define MSG_ADD_COURSE 12
#define MSG_UPDATE_COURSE 13
#define MSG_ENROLL_COURSE 14
#define MSG_UNENROLL_COURSE 15
#define MSG_LIST_COURSES 16
#define MSG_LIST_ENROLLMENTS 17
#define MSG_CHANGE_PASSWORD 18
#define MSG_GENERIC_RESPONSE 19
#define MSG_LIST_STUDENTS_IN_COURSE 20
#define MSG_DELETE_USER 21
#define MSG_PURGE_DELETED_USERS 23
#define MSG_ADD_COURSE_BEGIN 40
#define MSG_ADD_COURSE_END 41

// Generic request/response structs
// For add/update user
typedef struct {
    User user;
} UserRequest;

// For add/update course
typedef struct {
    Course course;
} CourseRequest;

// For enroll/unenroll
typedef struct {
    int student_id;
    int course_id;
} EnrollRequest;

// For generic response
typedef struct {
    int success;
    char message[MAX_LINE];
} GenericResponse;

#endif // COMMON_H 