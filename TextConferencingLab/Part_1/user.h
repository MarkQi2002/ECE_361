// Conditional Compilation
#ifndef _USER_H_
#define _USER_H_

// asser.h -> C Preprocessor Macro assert() That Implement Runtime Assertion
// stdio.h -> Standard Input And Output Library
// stdlib.h -> General Purpose Standard Library
// string.h -> String Library
// stdbool.h -> Boolean Type Variable Library
// pthread.h -> Linux Multithreading Library
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

// Defining Constants
#define USERNAME_LENGTH 64
#define PASSWORD_LENGTH 64
#define IP_LENGTH 16

// Define Session Structure
typedef struct Session {
    // Session ID
    int session_ID;

    // Linked List Support
    struct Session * next;
    struct User * user;
} Session;

// Define User Structure
typedef struct User {
    // Username And Password
    char username[USERNAME_LENGTH];
    char password[PASSWORD_LENGTH];

    // Usert Status
    // socketFD -> Socket File Descriptor
    // in_session -> Current In A Session Or Not
    // thread_id -> Identify User Thread
    int socketFD;
    bool in_session;
    pthread_t thread_id;

    // Linked List Support
    Session * session_joined;
    struct User * next;
} User;

// User Related Functions
// Add User To A List
User * add_user(User * user_list, User * new_user) {
    // Append user_list To new_user
    new_user -> next = user_list;

    // Return new_user
    return new_user;
}

// Remove User From List Based On Username
// User const * -> We Can Change The Pointer To Point To Any Other User, But Cannot Change Value Of Object Pointed Using user
User * remove_user(User * user_list, User const * user) {
    // Check If Empty User List
    if (user_list == NULL) {
        return NULL;
    // First In User List
    } else if (strcmp(user_list -> username, user -> username) == 0) {
        // Return New User List With First User Removed
        User * current_ptr = user_list -> next;
        free(user_list);
        return current_ptr;
    // Check If User In User List
    } else {
        // Variable Declaration
        User * current_ptr = user_list;
        User * previous_ptr = NULL;
        
        // Iterate Through Linked List
        while (current_ptr != NULL) {
            if (strcmp(current_ptr -> username, user -> username) == 0) {
                previous_ptr -> next = current_ptr -> next;
                free(current_ptr);
                return user_list;
            } else {
                previous_ptr = current_ptr;
                current_ptr = current_ptr -> next;
            }
        }
    }

    // Didn't Found User From User List Return NULL
    return NULL;
}

// Read User Database From File
User * initiate_userlist_from_file(FILE * fp) {
    // Variable Declaration
    int read;
    User * user_list = NULL;

    // Read From File Until EOF
    while (1) {
        // Read From File
        User * new_user = (User *) malloc(sizeof(User));
        read = fscanf(fp, "%s %s\n", new_user -> username, new_user -> password);
        if (read == EOF) {
            free(new_user);
            break;
        }

        // Append new_user To user_list
        user_list = add_user(user_list, new_user);
    }

    // Return User List
    return user_list;
}

// Free Users Linked List
void free_user_list(User * user_list) {
    User * current_ptr = user_list;
    User * previous_ptr = NULL;
    while (current_ptr != NULL) {
        previous_ptr = current_ptr;
        free(current_ptr);
        current_ptr = previous_ptr -> next;
    }
}

// Check Username And Password Of User
bool is_valid_user(const User * user_list, const User * user) {
    // Iterating Through User List
    const User * current_ptr = user_list;
    while (current_ptr != NULL) {
        // Valid User
        if (strcmp(current_ptr -> username, user -> username) == 0 && strcmp(current_ptr -> password, user -> password) == 0) {
            return 1;
        }
        current_ptr = current_ptr -> next;
    }

    // Invalid User
    return 0;
}

// Check User In User List
bool in_list(const User * user_list, const User * user) {
    // Iterating Through User List
    const User * current_ptr = user_list;
    while (current_ptr != NULL) {
        // In User List
        if (strcmp(current_ptr -> username, user -> username) == 0) {
            return 1;
        }
        current_ptr = current_ptr -> next;
    }

    // Not In User List
    return 0;
}

// Seesion Related Functions
// Check Valid Session
Session * is_valid_session(Session * session_list, int session_ID) {
    // Iterate Through Entire Session List
    Session * current_ptr = session_list;
    while (current_ptr != NULL) {
        if (current_ptr -> session_ID == session_ID) {
            return current_ptr;
        }
        current_ptr = current_ptr -> next;
    }

    // Didn't Found Session With Session ID
    return NULL;
}

// Check If User In Session ID
bool in_session(Session * session_list, int session_ID, const User * user) {
    // Check Valid Session
    Session * session = is_valid_session(session_list, session_ID);
    if (session != NULL) {
        return in_list(session -> user, user);
    } else {
        return 0;
    }
}

// Initialize New Session
Session * initiate_session(Session * session_list, const int session_ID) {
    // Variable Declaration
    Session * new_session = (Session *) malloc(sizeof(Session));
    new_session -> session_ID = session_ID;
    new_session -> next = session_list;
    return new_session;
}

// Insert User Into Session List
Session * join_session(Session * session_list, int session_ID, const User * user) {
    // Check Session Exist
    Session * current_session = is_valid_session(session_list, session_ID);
    assert(current_session != NULL);

    // Malloc New User
    User * new_user = (User *) malloc(sizeof(User));
    memcpy((void *)new_user, (void *) user, sizeof(User));

    // Insert To Session List
    current_session -> user = add_user(current_session -> user, new_user);

    // Return Session List
    return session_list;
}

// Remove User From List Based On Username
// User const * -> We Can Change The Pointer To Point To Any Other User, But Cannot Change Value Of Object Pointed Using user
Session * remove_session(Session * session_list, int session_ID) {
    // Check If Empty Session List
    assert(session_list != NULL);

    // First In Session List
    if (session_list -> session_ID == session_ID) {
        // Return New Session List With First User Removed
        Session * current_ptr = session_list -> next;
        free(session_list);
        return current_ptr;
    // Check If Session In Session List
    } else {
        // Variable Declaration
        Session * current_ptr = session_list;
        Session * previous_ptr = NULL;
        
        // Iterate Through Linked List
        while (current_ptr != NULL) {
            if (current_ptr -> session_ID == session_ID) {
                previous_ptr -> next = current_ptr -> next;
                free(current_ptr);
                return session_list;
            } else {
                previous_ptr = current_ptr;
                current_ptr = current_ptr -> next;
            }
        }
    }

    // Didn't Found Session From Session List Return NULL
    return NULL;
}

// User Leave Session
Session * leave_session(Session * session_list, int session_ID, const User * user) {
    // Check Session Exists Outside And Find Current Session
    Session * current_session = is_valid_session(session_list, session_ID);
    assert(current_session != NULL);

    // Remove User From Current Session
    assert(in_list(current_session -> user, user));
    current_session -> user = remove_user(current_session -> user, user);

    // Remove Session If No More User
    if (current_session -> user == NULL) {
        session_list = remove_session(session_list, session_ID);
    }

    // Return Final SessionList
    return session_list;
}

// Free Sessions Linked List
void free_session_list(Session * session_list) {
    Session * current_ptr = session_list;
    Session * next_ptr = NULL;
    while (current_ptr != NULL) {
        free_user_list(current_ptr -> user);
        next_ptr = current_ptr -> next;
        free(current_ptr);
        current_ptr = next_ptr;
    }
}

// Conditional Compilation
#endif