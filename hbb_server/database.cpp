#include "main.h"

Database::Database(void) {
    databaseMode = DATABASE_MODE_INVALID;
    SQLiteDB = NULL;
    allowExceptions = true;
}

bool Database::SQLiteOpen(std::string file) {
    databaseMode = DATABASE_MODE_SQLITE;
    if (sqlite3_open(file.c_str(), &SQLiteDB) == SQLITE_OK) {
        return true;
    }
    return false;
}

std::vector<std::vector<std::string>> Database::Execute(std::string statement) {
    if (databaseMode == DATABASE_MODE_SQLITE) {
        return SQLiteExecute(statement);
    }
    throw 1;
}

std::vector<std::vector<std::string>> Database::Execute(std::string statement, std::vector<std::string> params) {
    if (databaseMode == DATABASE_MODE_SQLITE) {
        return SQLiteExecute(statement, params);
    }
    throw 1;
}

std::vector<std::vector<std::string>> Database::SQLiteExecute(std::string statement) {
    std::vector<std::string> blank;
    return SQLiteExecute(statement, blank);
}

std::vector<std::vector<std::string>> Database::SQLiteExecute(std::string statement, std::vector<std::string> params) {
    std::vector<std::vector<std::string>> results;
    int result = -1;
    try {
        sqlite3_stmt *stmt;
        result = sqlite3_prepare_v2(SQLiteDB, statement.c_str(), (int)(statement.length()+1), &stmt, NULL);
        if (result != SQLITE_OK) {
            throw 1;
        }
        for (std::vector<std::string>::size_type i = 0; i < params.size(); i++) {
            sqlite3_bind_text(stmt, (int)i+1, params[i].c_str(), (int)(params[i].length()), SQLITE_STATIC);
        }
        int cols = sqlite3_column_count(stmt);
        while (true) {
            result = sqlite3_step(stmt);
            if(result == SQLITE_ROW) {
                std::vector<std::string> values;
                for(int col = 0; col < cols; col++) {
                    values.push_back((char*)sqlite3_column_text(stmt, col));
                }
                results.push_back(values);
            }
            else if (result == SQLITE_DONE) {
                break;
            }
            else {
                throw 1;
            }
        }
        sqlite3_finalize(stmt);
    }
    catch (...) {
		/*
        std::cerr << "Database error: " << result << " - " << sqlite3_errstr(result) << "\n";
        std::cerr << "Statement: " << statement << "\n";
        std::cerr << "Params:";
        for (unsigned int i = 0; i < params.size(); i++) {
            std::cerr << " " << params[i];
        }
        std::cerr << "\n";
		*/
        if (allowExceptions) {
            throw result;
        }
    }
    return results;
}

Database::~Database(void) {
}
