#define _GNU_SOURCE // needed for asprintf
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

#define FIVE_MINUTES 300000

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

    char *line = NULL;
    size_t size = 0;

    sqlite3_stmt *selectStatement;
    sqlite3_stmt *insertStatement;

    rc = sqlite3_prepare_v2(db, "SELECT * FROM plan WHERE start_time < ?1", -1, &selectStatement, 0); // preapares the selectStatement to fill in current time

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Error preparing selectStatement\n");
        sqlite3_close(db);
        return 1;
    }

    rc = sqlite3_prepare_v2(db, "INSERT INTO plan VALUES(NULL, ?1, ?2, ?3, ?4, ?5, ?6)", -1, &insertStatement, 0); // preapares the insertStatement to fill in values to insert

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Error preparing insertStatement\n");
        sqlite3_close(db);
        return 1;
    }

    printf("Available commands:\n\texit\n\tshow\n\tadd\n");

    while (true)
    {
        if(getline(&line, &size, stdin) == -1)
        {
            printf("No line.\n");
            break;
        }

        if(strncmp("exit", line, 4) == 0)
        {
            free(line);
            line = NULL;
            printf("Exiting.\n");
            break;
        }
        else if(strncmp("show", line, 4) == 0)
        {
            time_t curTime = time(NULL);

            sqlite3_bind_int64(selectStatement, 1, (sqlite3_int64) (curTime + FIVE_MINUTES)); // fill current time + five minutes

            rc = sqlite3_step(selectStatement);

            int colNum = sqlite3_column_count(selectStatement);

            if (rc == SQLITE_ROW)
            {
                for (int i = 0 ; i < colNum ; i++)
                {
                    printf("%s\t%s\n", sqlite3_column_name(selectStatement, i), sqlite3_column_text(selectStatement, i));
                }
            }

            continue;
        }
        else if(strncmp("add", line, 3) == 0)
        {
            // Object name
            printf("Provide object name: ");
            ssize_t chars_read = getline(&line, &size, stdin);

            if (line[chars_read - 1] == '\n') 
            {
                line[chars_read - 1] = '\0'; // Replace newline with null terminator
                chars_read--; // Decrement the count
            }

            sqlite3_bind_text(insertStatement, 1, line, -1, SQLITE_TRANSIENT);


            // Is interstellar
            printf("Is object interstellar? (0/1): ");
            getline(&line, &size, stdin);

            if (size > 0 && line[strlen(line) - 1] == '\n')
            {
                line[strlen(line) - 1] = '\0';
            }

            sqlite3_bind_int64(insertStatement, 2, strtol(line, NULL, 10));


            // Azimuth
            printf("Provide azimuth: ");
            getline(&line, &size, stdin);

            if (size > 0 && line[strlen(line) - 1] == '\n')
            {
                line[strlen(line) - 1] = '\0';
            }

            sqlite3_bind_double(insertStatement, 3, strtod(line, NULL));


            // Elevation
            printf("Provide elevation: ");
            getline(&line, &size, stdin);

            if (size > 0 && line[strlen(line) - 1] == '\n')
            {
                line[strlen(line) - 1] = '\0';
            }

            sqlite3_bind_double(insertStatement, 4, strtod(line, NULL));


            // Start time 
            printf("Provide start time (Unix Timestamp): ");
            getline(&line, &size, stdin);

            if (size > 0 && line[strlen(line) - 1] == '\n')
            {
                line[strlen(line) - 1] = '\0';
            }

            sqlite3_bind_int64(insertStatement, 5, strtoll(line, NULL, 10));


            // Duration minutes
            printf("Provide duration minutes: ");
            getline(&line, &size, stdin);

            if (size > 0 && line[strlen(line) - 1] == '\n')
            {
                line[strlen(line) - 1] = '\0';
            }

            sqlite3_bind_int64(insertStatement, 6, strtol(line, NULL, 10));

            rc = sqlite3_step(insertStatement);

            if (rc != SQLITE_DONE)
            {
                fprintf(stderr, "SQL error while inserting: %s\n", sqlite3_errmsg(db));
                break;
            }
            else
            {
                printf("Successfully added new plan entry.\n");
            }

            sqlite3_reset(insertStatement);
        
        }
    }

    sqlite3_close(db);

    return 0;
}