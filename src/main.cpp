#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct __attribute__((__packed__)) TimeRequest
{
    char protocol[3]; // Protocol name (TSP)
    uint8_t protocolVersion; // 1
    char unused[4]; // 8 bytes padding, can have future use
    uint64_t clientCookie; // 8 bytes which user can set to whatever value, and will be returned in reply
};

const int TimeRequestPacketSize = sizeof(TimeRequest);


struct __attribute__((__packed__)) TimeReply
{
    char protocol[3]; // Protocol name (TSP)
    uint8_t protocolVersion; // 1
    char unused[4]; // 8 bytes padding, can have future use
    uint64_t clientCookie; // the cookie which was sent in the request, copied to the reply for reference
    uint64_t timeSinceEphoc1970Ms; // number of ms since ephoc time - 1 Jan 1970 GMT
};

const int TimeReplyPacketSize = sizeof(TimeReply);

/*
 * error - wrapper for perror
 */
void error(const char *msg) {
  // perror(msg);
  exit(1);
}

static void becomeBackgroundProccess()
{
  pid_t pid = fork();
  if (pid < 0) // fork failed
  { 
    syslog(LOG_ERR, "daemonize: first fork failed");  
		exit(EXIT_FAILURE);
	}
	if (pid > 0) // parent terminates
  { 
    exit(EXIT_SUCCESS);
	}
}

static void becomeLeaderOfNewSession()
{
	if (setsid() < 0) 
  {
    syslog(LOG_ERR, "daemonize: setsid failed");  
		exit(EXIT_FAILURE);
	}
}

static void ignoreSigChldSignal()
{
	signal(SIGCHLD, SIG_IGN);
}

static void ensureNotSessionLeader()
{
	pid_t pid = fork();
	if (pid < 0) // second fork failed
  {
    syslog(LOG_ERR, "daemonize: second fork failed");  
		exit(EXIT_FAILURE);
	}
	if (pid > 0) // parent terminates
  {
		exit(EXIT_SUCCESS);
	}
}

// so if new files are created, they will have the correct permissions
static void clearUmask()
{
	umask(0);
}

// so we won't stuck if need to later unmount the FS which CWD is using
static void changeWorkingDirectory()
{
	chdir("/");
}

// they are not used in the daemon, and theredore should be cleaned
// to prevent problems with later unmounts and resource leak.
static void closeAllFileDescriptors()
{
  int maxfd = sysconf(_SC_OPEN_MAX);
  if (maxfd < 0) // limit is indeterminate
  {
    maxfd = 8192; // so take a guess
  }
  for (int fd = 0; fd < maxfd; fd++)
  {
    close(fd);
  }
}

// make the standart file descriptors (0, 1, 2)
// point to /dev/null, so if they are used by a library,
// the operation will not fail or write to some other open fd
static void redirectStdFdsToDevNull()
{
  int fd = open("/dev/null", O_RDWR);
  if (fd != STDIN_FILENO) // 'fd' should be 0
  {
    syslog(LOG_ERR, "daemonize: failed to open /dev/null");  
    exit(EXIT_FAILURE);
  }
  if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
  {
    syslog(LOG_ERR, "daemonize: failed to set /dev/null to STDOUT");  
    exit(EXIT_FAILURE);
  }
  if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
  {
    syslog(LOG_ERR, "daemonize: failed to open /dev/null to STDERR");  
    exit(EXIT_FAILURE);
  }
}

static void lockPidFile()
{
  const char *lockFilePath = "/var/run/tssd.pid";
  char str[256];
  int pidFd = open(lockFilePath, O_RDWR|O_CREAT, 0640);
  if (pidFd < 0) {
    syslog(LOG_ERR, "daemonize: cannot create lock file at '%s'", lockFilePath);
    exit(EXIT_FAILURE);
  }
  if (lockf(pidFd, F_TLOCK, 0) < 0) {
    /* Can't lock file */
    syslog(LOG_ERR, "daemonize: cannot lock the lock file at '%s'", lockFilePath);
    exit(EXIT_FAILURE);
  }
  /* Get current PID */
  sprintf(str, "%d\n", getpid());
  /* Write PID to lockfile */
  write(pidFd, str, strlen(str));  
}

static void daemonize()
{
	becomeBackgroundProccess();
  becomeLeaderOfNewSession();
  ignoreSigChldSignal();
  ensureNotSessionLeader();
  clearUmask();
  changeWorkingDirectory();
  closeAllFileDescriptors();
  redirectStdFdsToDevNull();
  lockPidFile();
}

int main(int argc, char **argv) 
{
  int sockfd; /* socket */
  int portno = 12321; /* port to listen on */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  char requestBuffer[TimeRequestPacketSize];
  char replyBuffer[TimeReplyPacketSize];
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  const char *appName = argv[0];

  /* 
   * check command line arguments 
   */
  if (argc == 2) 
  {
    portno = atoi(argv[1]);
  }

  if(argc > 2)
  {
    exit(1);
  }

  daemonize();

	/* Open system log and write message to it */
	openlog(argv[0], LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started time sync server '%s'", appName);  

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, check validite and response with the time
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(requestBuffer, TimeRequestPacketSize);
    n = recvfrom(sockfd, requestBuffer, TimeRequestPacketSize, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    struct timeval tv;
    gettimeofday(&tv, NULL);
    // convert sec to ms and usec to ms
    uint64_t curr_time_ms_since_epoch = ((uint64_t)(tv.tv_sec)) * 1000 + ((uint64_t)(tv.tv_usec)) / 1000;

    memcpy(replyBuffer, requestBuffer, TimeRequestPacketSize);
    ((TimeReply *)replyBuffer)->timeSinceEphoc1970Ms = curr_time_ms_since_epoch;
    n = sendto(sockfd, replyBuffer, TimeReplyPacketSize, MSG_CONFIRM, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
  }

	syslog(LOG_INFO, "stopped %s", appName);

}