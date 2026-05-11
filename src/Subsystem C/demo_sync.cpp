#include <iostream>
#include "sync.h"

// Subsystem C demo – Synchronization & Protection
// TODO: implement when sync.cpp is complete

int main() {
    SyncState state;

    int rc = sync_init(&state);
    std::cout << "init: " << rc << "\n";
    if (rc == 0) {
        rc = sync_begin_read(&state, ROLE_USER, 0);
        std::cout << "begin_read: " << rc << "\n";
        if (rc == 0) {
            rc = sync_end_read(&state);
            std::cout << "end_read: " << rc << "\n";
        }
        rc = sync_destroy(&state);
        std::cout << "destroy: " << rc << "\n";
    }


    std::cout << "-----\n";

    int wc = sync_init(&state);
    std::cout << "init: " << wc << "\n";
    if (wc == 0) {
        wc = sync_begin_write(&state, ROLE_EDITOR, 0);
        std::cout << "begin_write: " << wc << "\n";
        if (wc == 0) {
            wc = sync_end_write(&state);
            std::cout << "end_write: " << wc << "\n";
        }
        wc = sync_destroy(&state);
        std::cout << "destroy: " << wc << "\n";
    }

    std::cout << "-----\n";

    int we = sync_init(&state);
    std::cout << "init: " << we << "\n";
    if (we == 0) {
        we = sync_begin_write(&state, ROLE_USER, 0);
        std::cout << "begin_write: " << we << "\n";
        if (we == 0) {
            we = sync_end_write(&state);
            std::cout << "end_write: " << we << "\n";
        } else {
            std::cerr << "Failed to acquire write lock. Error code: " << we << "\n";
        }
        we = sync_destroy(&state);
        std::cout << "destroy: " << we << "\n";
    }

    return 0;
}
