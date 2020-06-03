#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <string.h>

#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>


#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>



static char *HOST = NULL;
static char *PORT = NULL;
static char *DIR = NULL;


static void parse_opt(int argc, char *argv[])
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
    if (!HOST)
        HOST = strdup("127.0.0.1");
    if (!PORT)
        PORT = strdup("8080");
    if (!DIR)
        DIR = strdup("/");
}


static void redirect_stdout_logfile()
{
    char path[BUFSIZ];
    snprintf(path, BUFSIZ, "/tmp/%d.log", getpid());
    int fd = open(path, O_CREAT | O_WRONLY | O_APPEND, 0666);
    dup2(fd, STDOUT_FILENO);
    //dup2(fd, STDERR_FILENO);
}


static void change_root_directory()
{
    chdir(DIR);
    if (chroot(DIR) != 0)
    {
        perror("chroot");
        exit(EXIT_FAILURE);
    }
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

    close(STDIN_FILENO);
    // close(STDOUT_FILENO);
    // close(STDERR_FILENO);

    /* Close all open file descriptors */
    // int x;
    // for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    // {
    //     close (x);
    // }

    // /* Open the log file */
    // openlog ("firstdaemon", LOG_PID, LOG_DAEMON);
}


static void signal_handler(int sig)
{
    printf("=== SIGCHLD = %d\n", sig);

    int status = 0;
    pid_t ret = waitpid(-1, &status, 0);
    printf("status = %d\n", status);
    printf("ret = %d\n", ret);
    printf("===\n");
}


static int connect_handler(int cs)
{
    char buf[BUFSIZ + 1];

    // REQUEST

    size_t size = 0;
    while (size < BUFSIZ)
    {
        ssize_t ret = read(cs, buf + size, BUFSIZ - size);
        if (ret > 0)
        {
            size += ret;
            buf[size] = '\0';
            if (strstr(buf, "\r\n\r\n") != NULL || strstr(buf, "\n\n") != NULL)
                break ;
        }
    }
    
    printf(">>> GET REQUEST\n%s\n<<< GET REQUEST\n", buf);
    if (0 != strncmp(buf, "GET ", 4))
    {
        printf("!!! ERROR GET #1\n");
        return (EXIT_FAILURE);
    }
    char *ptr0 = buf + 4;
    char *ptr1 = strchr(ptr0, ' ');
    if (!ptr1)
    {
        printf("!!! ERROR GET #2\n");
        return (EXIT_FAILURE);
    }
    char *path = strndup(ptr0, ptr1 - ptr0);
    
    printf("=== PATH = %s\n", path);

    if (0 != strncmp(ptr1 + 1, "HTTP/1.0", 8) && 0 != strncmp(ptr1 + 1, "HTTP/1.1", 8))
    {
        printf("!!! ERROR GET #3\n");
        return (EXIT_FAILURE);
    }
    ptr1 += 1 + 8;
    if (0 != strncmp(ptr1, "\r\n", 2) && 0 != strncmp(ptr1, "\n", 1))
    {
        printf("!!! ERROR GET #4\n");
        return (EXIT_FAILURE);
    }
    if (strstr(ptr1, "\r\n\r\n") == NULL && strstr(ptr1, "\n\n") == NULL)
    {
        printf("!!! ERROR GET #5\n");
        return (EXIT_FAILURE);
    }

    // RESPONSE

    struct stat st;
    if (-1 == stat(path, &st))
    {
        printf("!!! ERROR GET STAT\n");
        dprintf(cs, "HTTP/1.0 404 Not Found\r\n\r\n");
        return (EXIT_FAILURE);
    }

    if (!S_ISREG(st.st_mode))
    {
        printf("!!! ERROR GET S_ISREG\n");
        dprintf(cs, "HTTP/1.0 404 Not Found\r\n\r\n");
        return (EXIT_FAILURE);
    }

    dprintf(cs, "HTTP/1.0 200 OK\r\nContent-Length = %d\r\nContent-Type: text/html\r\n\r\n", st.st_size);

    printf("open path = %s\n", path);

    int fd = open(path, O_RDONLY);
    while (1)
    {
        ssize_t ret = read(fd, buf, BUFSIZ);
        if (ret <= 0) break ;
        write(cs, buf, ret);
        printf("write ret = %d\n", ret);
    }
    printf("close path = %s\n", path);
    close(fd);

    free(path);

    return (EXIT_SUCCESS);
}


static void run_server()
{
    struct sockaddr_in local;

	int ss = socket(AF_INET, SOCK_STREAM, 0);
	inet_aton(HOST, &local.sin_addr);
	local.sin_port = htons(atoi(PORT));
	local.sin_family = AF_INET;

	if (-1 == bind(ss, (struct sockaddr*) &local, sizeof(local)))
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if (-1 == listen(ss, 5))
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

    while (1) 
    {
        int cs = accept(ss, NULL, NULL);
        if (-1 == cs)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (-1 == pid)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        
        if (0 == pid)
        {
            int ret = connect_handler(cs);        
            shutdown(cs, SHUT_RDWR);
            close(cs);
            exit(ret);
        }    
    }
    close(ss);
}


int main(int argc, char **argv)
{
    parse_opt(argc, argv);
    redirect_stdout_logfile();
    change_root_directory();
    skeleton_daemon();
    
    signal(SIGCHLD, signal_handler);

    printf("=== START\n");
    printf("HOST = %s\n", HOST);
    printf("PORT = %s\n", PORT);
    printf("DIR = %s\n", DIR);

    run_server();

    free(HOST);
    free(PORT);
    free(DIR);

    return EXIT_SUCCESS;
}
