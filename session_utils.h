#ifndef SESSION_UTILS_H
#define SESSION_UTILS_H

int is_user_logged_in(int user_id);
void add_active_session(int user_id);
void remove_active_session(int user_id);

#endif // SESSION_UTILS_H 