#ifndef ADMIN_H
#define ADMIN_H
void admin_menu(int sock);
void add_user_prompt(int sock, UserRole role);
void activate_deactivate_student(int sock);
void update_user_details(int sock);
void delete_user_prompt(int sock);
#endif 