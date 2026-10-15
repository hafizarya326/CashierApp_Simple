// Database.h - everything that talks to MySQL goes through these functions.
#ifndef DATABASE_H
#define DATABASE_H

#include <winsock2.h>   // must come before mysql.h on Windows
#include <mysql.h>
#include <string>

bool ConnectDatabase(std::string& error);   // connect to XAMPP MySQL
void CloseDatabase();

bool RunQuery(const std::string& sql);      // INSERT / UPDATE / DELETE / START TRANSACTION ...
MYSQL_RES* GetResult(const std::string& sql); // SELECT (call mysql_free_result when done)

std::string Escape(const std::string& text);  // makes text safe to put inside an SQL string
long long LastInsertId();                     // id created by the last INSERT
long long AffectedRows();                     // rows changed by the last UPDATE/DELETE
std::string LastDbError();                    // text of the last MySQL error
unsigned int LastDbErrorCode();               // number of the last MySQL error

#endif
