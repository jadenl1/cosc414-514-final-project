#include <iostream>
#include "sync.h"

// ── Output helpers ────────────────────────────────────────────────────────────

static const char *role_name(sync_role_t r) {
    switch (r) {
        case ROLE_USER:   return "USER";
        case ROLE_EDITOR: return "EDITOR";
        case ROLE_ADMIN:  return "ADMIN";
        default:          return "UNKNOWN";
    }
}

static void check(const char *label, int rc) {
    std::cout << "  " << label << ": " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
}

// ── Main loop ─────────────────────────────────────────────────────────────────

int main() {
    std::cout << "MOSS Sync Demo\n";

    SyncState state;

    // Scenario 1: USER reads (allowed)
    std::cout << "\n-- Scenario 1: USER read --\n";
    check("init",       sync_init(&state));
    check("begin_read", sync_begin_read(&state, ROLE_USER, 0));
    check("end_read",   sync_end_read(&state));
    check("destroy",    sync_destroy(&state));

    // Scenario 2: EDITOR writes (allowed)
    std::cout << "\n-- Scenario 2: EDITOR write --\n";
    check("init",        sync_init(&state));
    check("begin_write", sync_begin_write(&state, ROLE_EDITOR, 0));
    check("end_write",   sync_end_write(&state));
    check("destroy",     sync_destroy(&state));

    // Scenario 3: USER tries to write (permission denied)
    std::cout << "\n-- Scenario 3: USER write (should be denied) --\n";
    check("init",        sync_init(&state));
    int rc = sync_begin_write(&state, ROLE_USER, 0);
    std::cout << "  begin_write as " << role_name(ROLE_USER)
              << ": " << (rc < 0 ? "DENIED" : "OK") << " (rc=" << rc << ")\n";
    check("destroy",     sync_destroy(&state));

    // Scenario 4: Permission check only
    std::cout << "\n-- Scenario 4: Permission checks --\n";
    sync_init(&state);
    struct { sync_role_t role; sync_action_t action; } checks[] = {
        {ROLE_USER,   ACTION_READ},
        {ROLE_USER,   ACTION_WRITE},
        {ROLE_USER,   ACTION_MANAGE},
        {ROLE_EDITOR, ACTION_WRITE},
        {ROLE_ADMIN,  ACTION_MANAGE},
    };
    for (auto &c : checks) {
        int r = sync_check_permission(&state, c.role, 0, c.action);
        std::cout << "  " << role_name(c.role)
                  << (c.action == ACTION_READ ? " READ  " :
                      c.action == ACTION_WRITE ? " WRITE " : " MANAGE")
                  << ": " << (r == 0 ? "ALLOWED" : "DENIED") << "\n";
    }
    sync_destroy(&state);

    return 0;
}
