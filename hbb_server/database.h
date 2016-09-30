#pragma once
#include "main.h"

#define DATABASE_MODE_INVALID 0
#define DATABASE_MODE_SQLITE 1
#define DATABASE_MODE_MYSQL 2

class Database {
private:
    sqlite3 *SQLiteDB;
public:
    int databaseMode;
    bool allowExceptions;
    
    Database(void);
    bool SQLiteOpen(std::string);
    std::vector<std::vector<std::string>> Execute(std::string);
    std::vector<std::vector<std::string>> Execute(std::string, std::vector<std::string>);
    std::vector<std::vector<std::string>> SQLiteExecute(std::string);
    std::vector<std::vector<std::string>> SQLiteExecute(std::string, std::vector<std::string>);
    ~Database(void);
};
