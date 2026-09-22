#include<sqlite3.h>
#include<stdio.h>

int main(void) {
    sqlite3 *db;
    if(sqlite3_open(":memory:", &db) != SQLITE_OK) {
        printf("Sqlite failed.\n");
        return 1;
    }
    sqlite3_close(db);
    return 0;
}
