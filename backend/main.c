#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define REQ_SIZE 32768
#define DB_FILE "battery.db"

static sqlite3 *db = NULL;

static double get_number(const char *body, const char *key, double fallback) {
    char needle[80];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(body, needle);
    if (!p) return fallback;
    p = strchr(p, ':');
    if (!p) return fallback;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    char *end = NULL;
    double value = strtod(p, &end);
    return end == p ? fallback : value;
}

static void init_db(void) {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS readings ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "temperature REAL NOT NULL,"
        "ambient REAL NOT NULL,"
        "current_a REAL NOT NULL,"
        "cooling REAL NOT NULL,"
        "status TEXT NOT NULL);";
    if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
        fprintf(stderr, "Database error: %s\n", sqlite3_errmsg(db));
        exit(1);
    }
    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        exit(1);
    }
}

static const char *status_for(double t) {
    if (t >= 60.0) return "CRITICAL";
    if (t >= 45.0) return "WARNING";
    if (t < 5.0) return "LOW";
    return "NORMAL";
}

static void send_response(int client, int code, const char *status,
                          const char *body, const char *content_type) {
    char header[1024];
    int len = (int)strlen(body);
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n\r\n",
        code, status, content_type, len);
    send(client, header, (size_t)hlen, 0);
    send(client, body, (size_t)len, 0);
}

static void save_reading(double temp, double ambient, double current, double cooling) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO readings(temperature,ambient,current_a,cooling,status) VALUES(?,?,?,?,?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, temp);
        sqlite3_bind_double(stmt, 2, ambient);
        sqlite3_bind_double(stmt, 3, current);
        sqlite3_bind_double(stmt, 4, cooling);
        sqlite3_bind_text(stmt, 5, status_for(temp), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
}

static void handle_client(int client) {
    char req[REQ_SIZE];
    ssize_t n = recv(client, req, sizeof(req) - 1, 0);
    if (n <= 0) return;
    req[n] = '\0';

    char method[8] = {0}, path[256] = {0};
    sscanf(req, "%7s %255s", method, path);

    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client, 204, "No Content", "", "application/json");
        return;
    }

    char *body = strstr(req, "\r\n\r\n");
    body = body ? body + 4 : (char *)"{}";

    if (strcmp(path, "/api/health") == 0) {
        send_response(client, 200, "OK", "{\"ok\":true,\"service\":\"EV Battery C API\"}", "application/json");
        return;
    }

    if (strcmp(path, "/api/simulate") == 0 && strcmp(method, "POST") == 0) {
        double current = get_number(body, "current", 180.0);
        double ambient = get_number(body, "ambient", 25.0);
        double initial = get_number(body, "initialTemp", 28.0);
        double cooling = get_number(body, "cooling", 22.0);
        double seconds = get_number(body, "duration", 60.0);

        if (current < 0 || current > 600 || ambient < -30 || ambient > 60 ||
            initial < -30 || initial > 100 || cooling < 1 || cooling > 100 ||
            seconds < 1 || seconds > 3600) {
            send_response(client, 400, "Bad Request",
                "{\"error\":\"Input out of range. Check current, temperature, cooling and duration.\"}",
                "application/json");
            return;
        }

        /* Lumped thermal model: m*Cp*dT/dt = I^2*R - h*(T - ambient). */
        const double resistance = 0.045; /* pack-equivalent internal resistance, ohms */
        const double thermal_mass = 280.0 * 900.0; /* kg * J/(kg K) */
        double temp = initial;
        int steps = (int)ceil(seconds);
        for (int i = 0; i < steps; i++) {
            double dt = seconds / steps;
            double heat_w = current * current * resistance;
            double cooling_w = cooling * (temp - ambient);
            temp += ((heat_w - cooling_w) / thermal_mass) * dt;
        }

        save_reading(temp, ambient, current, cooling);
        char out[512];
        snprintf(out, sizeof(out),
            "{\"temperature\":%.2f,\"ambient\":%.2f,\"current\":%.1f,"
            "\"cooling\":%.1f,\"duration\":%.0f,\"status\":\"%s\","
            "\"heatGenerationW\":%.1f}",
            temp, ambient, current, cooling, seconds, status_for(temp), current*current*resistance);
        send_response(client, 200, "OK", out, "application/json");
        return;
    }

    if (strcmp(path, "/api/readings") == 0 && strcmp(method, "GET") == 0) {
        char out[20000];
        size_t used = 0;
        used += (size_t)snprintf(out + used, sizeof(out) - used, "{\"readings\":[");
        sqlite3_stmt *stmt = NULL;
        const char *sql = "SELECT id,created_at,temperature,ambient,current_a,cooling,status FROM readings ORDER BY id DESC LIMIT 100";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            int first = 1;
            while (sqlite3_step(stmt) == SQLITE_ROW && used < sizeof(out) - 500) {
                const unsigned char *date = sqlite3_column_text(stmt, 1);
                const unsigned char *status = sqlite3_column_text(stmt, 6);
                used += (size_t)snprintf(out + used, sizeof(out) - used,
                    "%s{\"id\":%d,\"createdAt\":\"%s\",\"temperature\":%.2f,"
                    "\"ambient\":%.2f,\"current\":%.1f,\"cooling\":%.1f,\"status\":\"%s\"}",
                    first ? "" : ",", sqlite3_column_int(stmt,0),
                    date ? (const char *)date : "",
                    sqlite3_column_double(stmt,2), sqlite3_column_double(stmt,3),
                    sqlite3_column_double(stmt,4), sqlite3_column_double(stmt,5),
                    status ? (const char *)status : "UNKNOWN");
                first = 0;
            }
        }
        sqlite3_finalize(stmt);
        snprintf(out + used, sizeof(out) - used, "]}");
        send_response(client, 200, "OK", out, "application/json");
        return;
    }

    if (strcmp(path, "/api/reset") == 0 && strcmp(method, "POST") == 0) {
        sqlite3_exec(db, "DELETE FROM readings;", NULL, NULL, NULL);
        send_response(client, 200, "OK", "{\"ok\":true,\"message\":\"History cleared\"}", "application/json");
        return;
    }

    send_response(client, 404, "Not Found", "{\"error\":\"Route not found\"}", "application/json");
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    init_db();

    const char *port_env = getenv("PORT");
    int port = port_env ? atoi(port_env) : 8080;
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server); return 1;
    }
    if (listen(server, 16) < 0) { perror("listen"); close(server); return 1; }

    printf("EV Battery C API listening on port %d\n", port);
    while (1) {
        int client = accept(server, NULL, NULL);
        if (client < 0) { if (errno == EINTR) continue; perror("accept"); continue; }
        handle_client(client);
        close(client);
    }
    sqlite3_close(db);
    close(server);
    return 0;
}
