#include <iostream>
#include <sstream>
#include <string>
#include "sync.h"

// ── State ─────────────────────────────────────────────────────────────────────

static SyncState state;
static bool initialized = false;

// ── Helpers ───────────────────────────────────────────────────────────────────

static sync_role_t parse_role(const std::string &s) {
    if (s == "USER")   return ROLE_USER;
    if (s == "EDITOR") return ROLE_EDITOR;
    if (s == "ADMIN")  return ROLE_ADMIN;
    return ROLE_COUNT;
}

static sync_action_t parse_action(const std::string &s) {
    if (s == "READ")   return ACTION_READ;
    if (s == "WRITE")  return ACTION_WRITE;
    if (s == "MANAGE") return ACTION_MANAGE;
    return ACTION_COUNT;
}

static const char *role_str(sync_role_t r) {
    switch (r) {
        case ROLE_USER:   return "USER";
        case ROLE_EDITOR: return "EDITOR";
        case ROLE_ADMIN:  return "ADMIN";
        default:          return "UNKNOWN";
    }
}

static const char *action_str(sync_action_t a) {
    switch (a) {
        case ACTION_READ:   return "READ";
        case ACTION_WRITE:  return "WRITE";
        case ACTION_MANAGE: return "MANAGE";
        default:            return "UNKNOWN";
    }
}

static void print_status() {
    if (!initialized) {
        std::cout << "  (not initialized — run 'init' first)\n";
        return;
    }
    std::cout << "  Active readers:  " << state.active_readers  << "\n"
              << "  Active writers:  " << state.active_writers  << "\n"
              << "  Waiting writers: " << state.waiting_writers << "\n";
}

static void print_permissions() {
    if (!initialized) return;
    sync_role_t   roles[]   = { ROLE_USER, ROLE_EDITOR, ROLE_ADMIN };
    sync_action_t actions[] = { ACTION_READ, ACTION_WRITE, ACTION_MANAGE };
    std::cout << "  Role      READ    WRITE   MANAGE\n"
              << "  " << std::string(36, '-') << "\n";
    for (auto role : roles) {
        std::cout << "  " << std::left;
        std::cout.width(10); std::cout << role_str(role);
        for (auto action : actions) {
            int r = sync_check_permission(&state, role, 0, action);
            std::cout.width(8); std::cout << (r == 0 ? "YES" : "no");
        }
        std::cout << "\n";
    }
}

static void print_help() {
    std::cout <<
        "\n  Commands:\n"
        "    init                                    initialize sync state\n"
        "    read   <USER|EDITOR|ADMIN>              attempt a read  (begin + end)\n"
        "    write  <USER|EDITOR|ADMIN>              attempt a write (begin + end)\n"
        "    check  <USER|EDITOR|ADMIN> <READ|WRITE|MANAGE>  permission check only\n"
        "    permissions                             show full permission matrix\n"
        "    status                                  show active readers/writers\n"
        "    demo                                    run preset scenarios\n"
        "    reset                                   destroy and reinitialize\n"
        "    help\n"
        "    exit\n\n";
}

static void run_demo() {
    std::cout << "\n  Running preset scenarios...\n";

    // Scenario 1: USER reads (allowed)
    std::cout << "\n  -- Scenario 1: USER read --\n";
    SyncState s;
    sync_init(&s);
    int rc = sync_begin_read(&s, ROLE_USER, 0);
    std::cout << "    begin_read (USER):  " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
    if (rc == 0) {
        rc = sync_end_read(&s);
        std::cout << "    end_read:          " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
    }
    sync_destroy(&s);

    // Scenario 2: EDITOR writes (allowed)
    std::cout << "\n  -- Scenario 2: EDITOR write --\n";
    sync_init(&s);
    rc = sync_begin_write(&s, ROLE_EDITOR, 0);
    std::cout << "    begin_write (EDITOR): " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
    if (rc == 0) {
        rc = sync_end_write(&s);
        std::cout << "    end_write:            " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
    }
    sync_destroy(&s);

    // Scenario 3: USER tries to write (permission denied)
    std::cout << "\n  -- Scenario 3: USER write (permission denied) --\n";
    sync_init(&s);
    rc = sync_begin_write(&s, ROLE_USER, 0);
    std::cout << "    begin_write (USER): " << (rc < 0 ? "DENIED" : "OK") << " (rc=" << rc << ")\n";
    sync_destroy(&s);

    // Scenario 4: Full permission matrix
    std::cout << "\n  -- Scenario 4: Permission matrix --\n";
    sync_init(&s);
    sync_role_t   roles[]   = { ROLE_USER, ROLE_EDITOR, ROLE_ADMIN };
    sync_action_t actions[] = { ACTION_READ, ACTION_WRITE, ACTION_MANAGE };
    std::cout << "    Role      READ    WRITE   MANAGE\n"
              << "    " << std::string(36, '-') << "\n";
    for (auto role : roles) {
        std::cout << "    " << std::left;
        std::cout.width(10); std::cout << role_str(role);
        for (auto action : actions) {
            int r = sync_check_permission(&s, role, 0, action);
            std::cout.width(8); std::cout << (r == 0 ? "YES" : "no");
        }
        std::cout << "\n";
    }
    sync_destroy(&s);

    std::cout << "\n  Demo complete.\n";
}

// ── Main loop ─────────────────────────────────────────────────────────────────

int main() {
    std::cout << "MOSS Sync — Synchronization & Protection\n"
              << "Type 'help' for commands\n";

    std::string line;
    while (true) {
        std::cout << "sync> ";
        if (!std::getline(std::cin, line)) break;

        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd.empty()) continue;

        if (cmd == "exit" || cmd == "quit") {
            break;

        } else if (cmd == "help") {
            print_help();

        } else if (cmd == "init") {
            if (initialized) {
                std::cout << "  already initialized — use 'reset' to reinitialize\n";
                continue;
            }
            int rc = sync_init(&state);
            if (rc != 0) {
                std::cout << "  error: sync_init failed (rc=" << rc
                          << ") — ensure you are running on Ubuntu\n";
            } else {
                initialized = true;
                std::cout << "  sync state initialized\n";
            }

        } else if (cmd == "reset") {
            if (initialized) sync_destroy(&state);
            int rc = sync_init(&state);
            if (rc != 0) {
                std::cout << "  error: reinit failed (rc=" << rc << ")\n";
                initialized = false;
            } else {
                initialized = true;
                std::cout << "  sync state reset\n";
            }

        } else if (cmd == "read") {
            if (!initialized) { std::cout << "  error: run 'init' first\n"; continue; }
            std::string role_str_in;
            if (!(ss >> role_str_in)) {
                std::cout << "  usage: read <USER|EDITOR|ADMIN>\n"; continue;
            }
            sync_role_t role = parse_role(role_str_in);
            if (role == ROLE_COUNT) {
                std::cout << "  unknown role: " << role_str_in << "\n"; continue;
            }
            int rc = sync_begin_read(&state, role, 0);
            if (rc < 0) {
                std::cout << "  begin_read (" << role_str(role) << "): DENIED (rc=" << rc << ")\n";
            } else {
                std::cout << "  begin_read (" << role_str(role) << "): OK\n";
                rc = sync_end_read(&state);
                std::cout << "  end_read: " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
            }

        } else if (cmd == "write") {
            if (!initialized) { std::cout << "  error: run 'init' first\n"; continue; }
            std::string role_str_in;
            if (!(ss >> role_str_in)) {
                std::cout << "  usage: write <USER|EDITOR|ADMIN>\n"; continue;
            }
            sync_role_t role = parse_role(role_str_in);
            if (role == ROLE_COUNT) {
                std::cout << "  unknown role: " << role_str_in << "\n"; continue;
            }
            int rc = sync_begin_write(&state, role, 0);
            if (rc < 0) {
                std::cout << "  begin_write (" << role_str(role) << "): DENIED (rc=" << rc << ")\n";
            } else {
                std::cout << "  begin_write (" << role_str(role) << "): OK\n";
                rc = sync_end_write(&state);
                std::cout << "  end_write: " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
            }

        } else if (cmd == "check") {
            if (!initialized) { std::cout << "  error: run 'init' first\n"; continue; }
            std::string role_str_in, action_str_in;
            if (!(ss >> role_str_in >> action_str_in)) {
                std::cout << "  usage: check <USER|EDITOR|ADMIN> <READ|WRITE|MANAGE>\n"; continue;
            }
            sync_role_t   role   = parse_role(role_str_in);
            sync_action_t action = parse_action(action_str_in);
            if (role == ROLE_COUNT)     { std::cout << "  unknown role: "   << role_str_in   << "\n"; continue; }
            if (action == ACTION_COUNT) { std::cout << "  unknown action: " << action_str_in << "\n"; continue; }
            int rc = sync_check_permission(&state, role, 0, action);
            std::cout << "  " << role_str(role) << " " << action_str(action)
                      << ": " << (rc == 0 ? "ALLOWED" : "DENIED") << "\n";

        } else if (cmd == "permissions") {
            if (!initialized) { std::cout << "  error: run 'init' first\n"; continue; }
            print_permissions();

        } else if (cmd == "status") {
            print_status();

        } else if (cmd == "demo") {
            run_demo();

        } else {
            std::cout << "  unknown command: " << cmd << " (type 'help')\n";
        }
    }

    if (initialized) sync_destroy(&state);
    return 0;
}
