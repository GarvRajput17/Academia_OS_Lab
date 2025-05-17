#include "common.h"
#include "syscalls.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Admin client menu and actions for Academia Portal
void send_user_request(int sock, int msg_type, User *user) {
    write(sock, &msg_type, sizeof(int));
    UserRequest req = {0};
    req.user = *user;
    write(sock, &req, sizeof(UserRequest));
    GenericResponse resp;
    read(sock, &resp, sizeof(GenericResponse));
    printf("%s\n", resp.message);
}

void add_user_prompt(int sock, UserRole role) {
    User user = {0};
    printf("Enter ID: ");
    scanf("%d", &user.id);
    getchar();
    printf("Enter Name: ");
    fgets(user.name, MAX_NAME_LEN, stdin);
    user.name[strcspn(user.name, "\n")] = 0;
    printf("Enter Password: ");
    fgets(user.password, MAX_PASS_LEN, stdin);
    user.password[strcspn(user.password, "\n")] = 0;
    user.active = 1;
    user.role = role;
    write(sock, &(int){MSG_ADD_USER}, sizeof(int));
    UserRequest req = {0};
    req.user = user;
    write(sock, &req, sizeof(UserRequest));
    GenericResponse resp;
    read(sock, &resp, sizeof(GenericResponse));
    if (resp.success == -2) {
        printf("Duplicate user ID or username. User not added.\n");
    } else {
        printf("%s\n", resp.message);
    }
}

void activate_deactivate_student(int sock) {
    int id;
    printf("Enter Student ID to activate/deactivate: ");
    scanf("%d", &id);
    getchar();
    User user = {0};
    user.id = id;
    send_user_request(sock, MSG_UPDATE_USER, &user); // Server will toggle active
}

void update_user_details(int sock) {
    int id;
    printf("Enter User ID to update: ");
    scanf("%d", &id);
    getchar();
    User user = {0};
    user.id = id;
    printf("Enter new name (or press Enter to keep): ");
    char buf[MAX_NAME_LEN];
    fgets(buf, MAX_NAME_LEN, stdin);
    if (buf[0] != '\n') {
        buf[strcspn(buf, "\n")] = 0;
        strncpy(user.name, buf, MAX_NAME_LEN);
    }
    printf("Enter new password (or press Enter to keep): ");
    fgets(buf, MAX_PASS_LEN, stdin);
    if (buf[0] != '\n') {
        buf[strcspn(buf, "\n")] = 0;
        strncpy(user.password, buf, MAX_PASS_LEN);
    }
    send_user_request(sock, MSG_UPDATE_USER, &user);
}

void delete_user_prompt(int sock) {
    int id;
    printf("Enter User ID to delete: ");
    scanf("%d", &id);
    getchar();
    write(sock, &(int){MSG_DELETE_USER}, sizeof(int));
    write(sock, &id, sizeof(int));
    GenericResponse resp;
    read(sock, &resp, sizeof(GenericResponse));
}

void change_admin_password(int sock) {
    write(sock, &(int){MSG_CHANGE_PASSWORD}, sizeof(int));
    UserRequest req = {0};
    req.user.id = 1; // admin id is always 1
    printf("Enter new admin password: ");
    char buf[MAX_PASS_LEN];
    fgets(buf, MAX_PASS_LEN, stdin);
    buf[strcspn(buf, "\n")] = 0;
    strncpy(req.user.password, buf, MAX_PASS_LEN);
    write(sock, &req, sizeof(UserRequest));
    GenericResponse resp;
    read(sock, &resp, sizeof(GenericResponse));
    printf("%s\n", resp.message);
}

void modify_user_details(int sock, UserRole role) {
    int id;
    printf("Enter %s ID to modify: ", role == ROLE_STUDENT ? "Student" : "Faculty");
    scanf("%d", &id);
    getchar();
    User user = {0};
    user.id = id;
    printf("Enter new name (or press Enter to keep): ");
    char buf[MAX_NAME_LEN];
    fgets(buf, MAX_NAME_LEN, stdin);
    if (buf[0] != '\n') {
        buf[strcspn(buf, "\n")] = 0;
        strncpy(user.name, buf, MAX_NAME_LEN);
    }
    printf("Enter new password (or press Enter to keep): ");
    fgets(buf, MAX_PASS_LEN, stdin);
    if (buf[0] != '\n') {
        buf[strcspn(buf, "\n")] = 0;
        strncpy(user.password, buf, MAX_PASS_LEN);
    }
    user.role = role;
    send_user_request(sock, MSG_UPDATE_USER, &user);
}

void block_activate_student(int sock, int block) {
    int id;
    printf("Enter Student ID to %s: ", block ? "block (deactivate)" : "activate");
    scanf("%d", &id);
    getchar();
    User user = {0};
    user.id = id;
    user.active = block ? 0 : 1;
    user.role = ROLE_STUDENT;
    send_user_request(sock, MSG_UPDATE_USER, &user);
}

void admin_menu(int sock) {
    int choice;
    while (1) {
        printf("\nAdmin Menu:\n");
        printf("1. Add Student\n");
        printf("2. Add Faculty\n");
        printf("3. Modify Student Details\n");
        printf("4. Modify Faculty Details\n");
        printf("5. Block (Deactivate) Student\n");
        printf("6. Activate Student\n");
        printf("7. Delete User\n");
        printf("8. Change Admin Password\n");
        printf("9. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        getchar();
        switch (choice) {
            case 1:
                add_user_prompt(sock, ROLE_STUDENT);
                break;
            case 2:
                add_user_prompt(sock, ROLE_FACULTY);
                break;
            case 3:
                modify_user_details(sock, ROLE_STUDENT);
                break;
            case 4:
                modify_user_details(sock, ROLE_FACULTY);
                break;
            case 5:
                block_activate_student(sock, 1);
                break;
            case 6:
                block_activate_student(sock, 0);
                break;
            case 7:
                delete_user_prompt(sock);
                break;
            case 8:
                change_admin_password(sock);
                break;
            case 9:
                return;
            default:
                printf("Invalid choice.\n");
        }
    }
} 