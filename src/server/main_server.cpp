#include <iostream>

#include "server/database_server.h"

int main() {
    db::server::DatabaseServer server;

    if (!server.start("0.0.0.0", 8080)) {
        std::cerr << "Failed to start database server.\n";
        return 1;
    }

    std::cout << "Database server skeleton started on http://127.0.0.1:8080\n";
    std::cout << "HTTP endpoints will be wired in the next frontend slice.\n";

    return 0;
}
