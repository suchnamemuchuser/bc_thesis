#define _GNU_SOURCE // needed for asprintf
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

int main()
{
    sqlite3* db;

    int rc = sqlite3_open("plan.db", &db);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Error opening db\n");
        sqlite3_close(db);
        return 1;
    }
    else
    {
        printf("Database succesfully opened.\n");
    }

    // TODO: try some CRUD on the db, change time in db to unix time, try automating?z

    time_t curTime = time(NULL);

    sqlite3_stmt *statement;

    rc = sqlite3_prepare_v2(db, "SELECT * FROM plan WHERE start_time < ?1", -1, &statement, 0);

    sqlite3_bind_int64(statement, 1, (sqlite3_int64) curTime);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Error preparing statement\n");
        sqlite3_close(db);
        return 1;
    }

    rc = sqlite3_step(statement);

    int colNum = sqlite3_column_count(statement);
    if (rc == SQLITE_ROW) {
        for (int i = 0 ; i < colNum ; i++)
        {
            printf("%s\n", sqlite3_column_text(statement, i));
        }
    }

    sqlite3_close(db);

    return 0;
}