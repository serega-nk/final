// #include <stdio.h>


// int main()
// {
//     puts("Hello world!sss\n");
//     return 0;
// }

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>


static const char OK[] = "HTTP/1.0 200 OK\r\nContent-length: %d\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n%s";
static const char NOT_FOUND[] = "HTTP/1.0 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n";


char *HOST = NULL;
char *PORT = NULL;
char *DIR = NULL;


void parse_opt(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "h:p:d:")) != -1) {
        switch(opt) {
        case 'h':
            HOST = strdup(optarg);
            break;
        case 'p':
            PORT = strdup(optarg);
            break;
        case 'd':
            DIR = strdup(optarg);
            break;
        }
    }
    // if (!HOST || !PORT || !DIR)
    //     exit(-1);
    if (!HOST)
        HOST = strdup("127.0.0.1");
    if (!PORT)
        PORT = strdup("8080");
    if (!DIR)
        DIR = strdup("/");
}



static void skeleton_daemon()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }

    // /* Open the log file */
    // openlog ("firstdaemon", LOG_PID, LOG_DAEMON);
}


static void child_handler(int sig)
{

    printf("SIGCHLD = %d\n", sig);

    int status = 0;
    pid_t ret = waitpid(-1, &status, 0);
    printf("status = %d\n", status);
    printf("ret = %d\n", ret);

}

static void client_handler(int cs)
{

    char buf[BUFSIZ + 1];
    char path[BUFSIZ + 1];
    char uri[BUFSIZ + 1];
    ssize_t len = read(cs, buf, BUFSIZ);
    buf[len] = '\0';

    FILE *log = fopen("/tmp/1.log", "a");
    fwrite(buf, 1, len, log);
    fclose(log);

        
    if (1 != sscanf(buf, "GET %s HTTP/1.0", &uri))
    {
        write(cs, NOT_FOUND, sizeof(NOT_FOUND));
        return ;
    }

    // if (strcmp(uri, "/") == 0)
    // {
    //     write(cs, NOT_FOUND, sizeof(NOT_FOUND));
    //     return ;
    // }

    char *ptr = path;
    strcat(ptr, DIR);
    ptr += strlen(DIR);
    strcat(ptr, "/./");
    ptr += strlen("/./");
    strcat(ptr, uri);
    ptr += strlen(uri);

    // write(cs, path, strlen(path));
    // write(cs, "\r\n\r\n", 4);

    struct stat st;
    if (-1 == stat(path, &st))
    {
        write(cs, NOT_FOUND, sizeof(NOT_FOUND));
        return ;
    }

    if (!S_ISREG(st.st_mode))
    {
        write(cs, NOT_FOUND, sizeof(NOT_FOUND));
        return ;
    }
    ssize_t length = st.st_size;

    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        write(cs, NOT_FOUND, sizeof(NOT_FOUND));
        return ;
    }



    size_t ret;
    char OK[] = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";

    //ret = snprintf(buf, BUFSIZ, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
    write(cs, OK, sizeof(OK));
    while ((ret = fread(buf, sizeof(char), BUFSIZ, fp)) > 0)
    {
        write(cs, buf, ret);
    }
    write(cs, "\r\n\r\n", 4);
}

int main(int argc, char *argv[]) {

    parse_opt(argc, argv);

    FILE *log = fopen("/tmp/1.log", "a");
    fprintf(log, "HOST: %s\n", HOST);
    fprintf(log, "PORT: %s\n", PORT);
    fprintf(log, "DIR: %s\n", DIR);
    fclose(log);
    
    skeleton_daemon();

    chdir(DIR);

    signal(SIGCHLD, child_handler);

    printf("HOST: %s\n", HOST);
    printf("PORT: %s\n", PORT);
    printf("DIR: %s\n", DIR);


    struct sockaddr_in local;

	int ss = socket(AF_INET, SOCK_STREAM, 0);
	inet_aton(HOST, &local.sin_addr);
	local.sin_port = htons(atoi(PORT));
	local.sin_family = AF_INET;


	if (-1 == bind(ss, (struct sockaddr*) &local, sizeof(local)))
	{
		printf("bind error\n");
		exit(EXIT_FAILURE);
	}

	if (-1 == listen(ss, 5))
	{
		printf("listen error\n");
		exit(EXIT_FAILURE);
	}

    while (1) 
    {
        int cs = accept(ss, NULL, NULL);
        if (-1 == cs)
        {
            printf("accept error\n");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (-1 == pid)
        {
            close(cs);
            continue;
        }
        if (0 == pid)
        {
            client_handler(cs);        
            shutdown(cs, SHUT_RDWR);
            close(cs);
            return EXIT_SUCCESS;
        }
    
    }
    close(ss);
    
    free(HOST);
    free(PORT);
    free(DIR);

    return EXIT_SUCCESS;
}