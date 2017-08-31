/*
 * Connect to DB-replica. Get last applied xlog location, emit filename and offset.
 *
 * I'm not sure if there is an easy way to get the
 *
 * PG connection code is lifted nearly verbatim from oid2name.c in PG contrib
 *
 * Originally by
 * B. Palmer, bpalmer@crimelabs.net 1-17-2001
 */
#include <pg_config.h>
#include "libpq-fe.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define XLogSegmentsPerXLogId	((uint64_t)(0x100000000) / XLOG_SEG_SIZE)

/* these are the opts structures for command line params */
struct options
{
    char *dbname;
    char *hostname;
    char *port;
    char *username;
    const char *progname;

    int timeline;
};


static void
usage(const char *progname)
{
    printf("%s helps determine where a PostgreSQL replica is in its replay of WAL\n\n"
        "Usage:\n"
        "  %s [OPTION]...\n"
        "\nOptions:\n"
        "  -d DBNAME      database to connect to\n"
        "  -H HOSTNAME    database server host or socket directory\n"
        "  -p PORT        database server port number\n"
        "  -U NAME        connect as specified database user\n"
        "  -V, --version  output version information, then exit\n"
        "  -?, --help     show this help, then exit\n",
        progname, progname);
}

/* function to parse command line options and check for some usage errors. */
static void
get_opts(int argc, char **argv, struct options *my_opts)
{
    int			c;
    const char *progname;

    progname = argv[0];

    /* set the defaults */
    my_opts->dbname = NULL;
    my_opts->hostname = NULL;
    my_opts->port = NULL;
    my_opts->username = NULL;
    my_opts->timeline = 1;
    my_opts->progname = progname;

    if (argc > 1)
    {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
        {
            usage(progname);
            exit(0);
        }
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
        {
            printf("%s (PostgreSQL) %s\n", progname, PG_VERSION);
            exit(0);
        }
    }

    /* get opts */
    while ((c = getopt(argc, argv, "H:p:U:d:t:h")) != -1)
    {
        switch (c)
        {
            /* specify the database */
            case 'd':
                my_opts->dbname = strdup(optarg);
                break;

                /* host to connect to */
            case 'H':
                my_opts->hostname = strdup(optarg);
                break;

                /* port to connect to on remote host */
            case 'p':
                my_opts->port = strdup(optarg);
                break;

                /* username */
            case 'U':
                my_opts->username = strdup(optarg);
                break;

            case 'h':
                usage(progname);
                exit(0);
                break;
            case 't':
                my_opts->timeline = atoi(optarg);
                break;

            default:
                fprintf(stderr, "Try \"%s --help\" for more information.\n", progname);
                exit(1);
        }
    }
}

/* establish connection with database. */
PGconn *
sql_conn(struct options *my_opts)
{
    PGconn	   *conn;
    int		have_password = 0;
    char		password[100];
    int		new_pass;

    /*
     * Start the connection.  Loop until we have a password if requested by
     * backend.
     */
    do
    {
#define PARAMS_ARRAY_SIZE	7

        const char *keywords[PARAMS_ARRAY_SIZE];
        const char *values[PARAMS_ARRAY_SIZE];

        keywords[0] = "host";
        values[0] = my_opts->hostname;
        keywords[1] = "port";
        values[1] = my_opts->port;
        keywords[2] = "user";
        values[2] = my_opts->username;
        keywords[3] = "password";
        values[3] = have_password ? password : NULL;
        keywords[4] = "dbname";
        values[4] = my_opts->dbname;
        keywords[5] = "fallback_application_name";
        values[5] = my_opts->progname;
        keywords[6] = NULL;
        values[6] = NULL;

        new_pass = 0;
        conn = PQconnectdbParams(keywords, values, 1);

        if (!conn)
        {
            fprintf(stderr, "could not connect to database %s\n", my_opts->dbname);
            exit(1);
        }

        if (PQstatus(conn) == CONNECTION_BAD &&
            PQconnectionNeedsPassword(conn) &&
            !have_password)
        {
            PQfinish(conn);
/*
            simple_prompt("Password: ", password, sizeof(password), 0);
            have_password = 1;
            new_pass = 1;
*/
            fprintf(stderr, "requires pw, not supported\n");
            exit(1);
        }
    } while (new_pass);

    /* check to see that the backend connection was successfully made */
    if (PQstatus(conn) == CONNECTION_BAD)
    {
        fprintf(stderr, "could not connect to database %s: %s",
            my_opts->dbname, PQerrorMessage(conn));
        PQfinish(conn);
        exit(1);
    }

    /* return the conn if good */
    return conn;
}

/*
 * Actual code to make call to the database and print the output data.
 */
static char *
sql_exec(PGconn *conn, const char *todo)
{
    PGresult   *res;

    int			nfields;
    int			nrows;
    char *out;

    /* make the call */
    res = PQexec(conn, todo);

    /* check and deal with errors */
    if (!res || PQresultStatus(res) > 2)
    {
        fprintf(stderr, "query failed: %s\n", PQerrorMessage(conn));
        fprintf(stderr, "query was: %s\n", todo);

        PQclear(res);
        PQfinish(conn);
        exit(-1);
    }

    /* get the number of fields */
    nrows = PQntuples(res);
    nfields = PQnfields(res);

    // return the first row of output
    out = strdup(PQgetvalue(res, 0, 0));

        /* cleanup */
    PQclear(res);
    return out;
}

int main(int argc, char **argv)
{
    	struct options *my_opts;
	PGconn	   *pgconn;
        char *loc=NULL;
        char *slash=NULL;
        long segment=0;
        long offset=0;

	my_opts = (struct options *) malloc(sizeof(struct options));
        	get_opts(argc, argv, my_opts);

	if (my_opts->dbname == NULL)
	{
		my_opts->dbname = "postgres";
	}
	pgconn = sql_conn(my_opts);

        if (pgconn == NULL) {
            fprintf(stderr, "couldn't connect\n");
            exit(1);
        }

        // should check the result here, to verify that we're talking to a replica.
        // sql_exec(pgconn, "select pg_is_in_recovery();");

        loc = sql_exec(pgconn, "select pg_last_xlog_replay_location();");
        slash = strchr(loc, '/');
        if (!slash) {
            fprintf(stderr, "can't find sentinel\n");
            exit(1);
        }
        *slash = '\0';
        segment = strtol(loc, NULL, 16);
        offset = strtol(slash+1, NULL, 16);
        printf("%08X%08X%08X %d", my_opts->timeline, segment, offset/ XLOG_SEG_SIZE, offset % XLOG_SEG_SIZE);
        free(loc);

        PQfinish(pgconn);
        exit(0);
}
