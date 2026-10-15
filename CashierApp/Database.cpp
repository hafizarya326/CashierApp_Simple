// Database.cpp - connection to XAMPP MySQL using the MariaDB C connector.
#include "Database.h"
#include <vector>

// ---- Connection settings (default XAMPP). Change here if yours are different. ----
static const char*  DB_HOST     = "127.0.0.1";   // same as "localhost"
static const unsigned int DB_PORT = 3306;
static const char*  DB_USER     = "root";
static const char*  DB_PASSWORD = "";            // XAMPP default: empty
static const char*  DB_NAME     = "cashier_db";

static MYSQL* conn = NULL;          // the one connection used by the whole program
static std::string lastError;
static unsigned int lastErrorCode = 0;

bool ConnectDatabase(std::string& error)
{
    conn = mysql_init(NULL);
    if (conn == NULL) {
        error = "Cannot initialize MySQL library.";
        return false;
    }

    unsigned int timeout = 5;                       // give up after 5 seconds
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    my_bool ssl_enforce = 0;
    mysql_options(conn, MYSQL_OPT_SSL_ENFORCE, &ssl_enforce);
    mysql_options(conn, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &ssl_enforce);

    // Try to connect. If it returns NULL, the connection failed.
    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASSWORD, DB_NAME, DB_PORT, NULL, 0) == NULL) {
        error = mysql_error(conn);
        mysql_close(conn);
        conn = NULL;
        return false;
    }
    mysql_set_character_set(conn, "utf8mb4");
    return true;
}

void CloseDatabase()
{
    if (conn != NULL) {
        mysql_close(conn);
        conn = NULL;
    }
}

bool RunQuery(const std::string& sql)
{
    if (conn == NULL) {
        lastError = "Not connected to MySQL.";
        return false;
    }
    if (mysql_query(conn, sql.c_str()) != 0) {      // 0 = success
        lastError = mysql_error(conn);
        lastErrorCode = mysql_errno(conn);
        return false;
    }
    return true;
}

MYSQL_RES* GetResult(const std::string& sql)
{
    if (!RunQuery(sql))
        return NULL;
    return mysql_store_result(conn);                // all rows of the SELECT
}

// Protects against SQL injection: turns ' into \' and so on.
std::string Escape(const std::string& text)
{
    if (conn == NULL)
        return "";
    std::vector<char> buffer(text.size() * 2 + 1);
    unsigned long length = mysql_real_escape_string(conn, &buffer[0], text.c_str(), (unsigned long)text.size());
    return std::string(&buffer[0], length);
}

long long LastInsertId()  { return (long long)mysql_insert_id(conn); }
long long AffectedRows()  { return (long long)mysql_affected_rows(conn); }
std::string LastDbError() { return lastError; }
unsigned int LastDbErrorCode() { return lastErrorCode; }
