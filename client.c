#include "socket_utils.h"
#include "syscalls.h"
#include "common.h"
#include "admin.h"
#include "student.h"
#include "faculty.h"
#include <string.h>
#include <stdio.h>

void print_startup_menu() {
    printf("\nWelcome to Academia Portal\n");
    printf("1. Login as Student\n");
    printf("2. Login as Faculty\n");
    printf("3. Login as Admin\n");
    printf("4. Quit\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }
    while (1) {
        int role_choice = 0;
        while (role_choice < 1 || role_choice > 4) {
            print_startup_menu();
            printf("Enter choice: ");
            scanf("%d", &role_choice);
            getchar();
        }
        if (role_choice == 4) return 0;
        int sock = create_client_socket(argv[1]);
        printf("Connected to server.\n");
        // Login prompt
        LoginRequest req;
        if (role_choice == 1) printf("Student Username: ");
        else if (role_choice == 2) printf("Faculty Username: ");
        else printf("Admin Username: ");
        fgets(req.name, MAX_NAME_LEN, stdin);
        req.name[strcspn(req.name, "\n")] = 0;
        printf("Password: ");
        fgets(req.password, MAX_PASS_LEN, stdin);
        req.password[strcspn(req.password, "\n")] = 0;

        // Send login request
        int msg_type = MSG_LOGIN_REQUEST;
        write(sock, &msg_type, sizeof(int));
        write(sock, &req, sizeof(LoginRequest));

        // Receive login response
        LoginResponse resp;
        read(sock, &resp, sizeof(LoginResponse));
        if (!resp.success) {
            printf("Login failed: %s\n", resp.message);
            close(sock);
            continue; // Prompt again
        }
        printf("Login successful! Welcome, %s\n", req.name);

        // Call role-based menu
        int done = 0;
        while (!done) {
            switch (resp.role) {
                case ROLE_ADMIN:
                    admin_menu(sock);
                    done = 1;
                    break;
                case ROLE_STUDENT:
                    student_menu(sock, resp.user_id);
                    done = 1;
                    break;
                case ROLE_FACULTY:
                    faculty_menu(sock, resp.user_id);
                    done = 1;
                    break;
                default:
                    printf("Unknown role. Try logging in again.\n");
                    done = 1;
            }
        }
        close(sock);
        // After menu, break to allow re-login or exit
        break;
    }
    return 0;
} 