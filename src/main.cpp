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

#include <cxxopts/cxxopts.hpp>

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


static volatile sig_atomic_t gotSigTerm = 0;

void handleSignal(int sig)
{
  syslog(LOG_INFO, "Singal handler %d", sig);  
  if (sig == SIGTERM)
  {
    gotSigTerm = 1;  
    signal(SIGTERM, SIG_DFL);
  }
}

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

static void lockPidFile(const char *pidfile)
{
  char str[256];
  int pidFd = open(pidfile, O_RDWR|O_CREAT, 0640);
  if (pidFd < 0) {
    syslog(LOG_ERR, "daemonize: cannot create lock file at '%s'", pidfile);
    exit(EXIT_FAILURE);
  }
  if (lockf(pidFd, F_TLOCK, 0) < 0) {
    /* Can't lock file */
    syslog(LOG_ERR, "daemonize: cannot lock the lock file at '%s'", pidfile);
    exit(EXIT_FAILURE);
  }
  /* Get current PID */
  sprintf(str, "%d\n", getpid());
  /* Write PID to lockfile */
  write(pidFd, str, strlen(str));  
}

static void daemonize(const char *pidfile)
{
	becomeBackgroundProccess();
  becomeLeaderOfNewSession();
  ignoreSigChldSignal();
  ensureNotSessionLeader();
  clearUmask();
  changeWorkingDirectory();
  closeAllFileDescriptors();
  redirectStdFdsToDevNull();
  lockPidFile(pidfile);
}

static cxxopts::ParseResult parseOptions(int argc, char **argv, cxxopts::Options &options)
{
  const char *appName = argv[0];

  options.add_options()
    ("h, help", "print help")
    ;

  try
  {
    cxxopts::ParseResult optsResult = options.parse(argc, argv);

    if(optsResult.count("help") > 0)
    {
      std::cout << options.help() << std::endl;
      exit(EXIT_SUCCESS);
    }

    return optsResult;
  }
  catch(const cxxopts::OptionException &e)
  {
    std::cerr << appName << ": " << e.what() << std::endl;
    std::cerr << "Try '" << appName << " --help' for more information." << std::endl;
    exit(EXIT_FAILURE);
  }
  catch(const std::exception& e)
  {
    std::cerr << appName << ": " << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char **argv) 
{
  const char *appName = argv[0];

  cxxopts::Options options(appName, "Time Sync Server Daemon: ntp like server, used to synchronize clients time fast and precisely");
  options.add_options()
    ("p, pidfile", "path referring to the systemd PID file of the service", cxxopts::value<std::string>()->default_value("/var/run/tssd.pid"))
    ;
  cxxopts::ParseResult parseResult = parseOptions(argc, argv, options);

  std::string pidfile;
  if(parseResult.count("pidfile") > 0)
  {
    pidfile = parseResult["pidfile"].as<std::string>();
  }

  
  int sockfd = -1;
  int portno = 12321; /* port to listen on */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  char requestBuffer[TimeRequestPacketSize];
  char replyBuffer[TimeReplyPacketSize];
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  daemonize(pidfile.c_str());

	/* Open system log and write message to it */
	openlog(argv[0], LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started time sync server daemon '%s'", appName);  

  signal(SIGTERM, handleSignal);

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
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

  /*
  Set timeout on the socket. it is good for 2 reasons:
  1. if we get a signal to terminate the service, this will give us a chance to 
    observe the flag change and exit the loop
  2. the code (which should react fast to time request) will be "hot" in cache,
    thus, decreasing the response time
  */
  struct timeval tvForSockRecv;
  tvForSockRecv.tv_sec = 0;
  tvForSockRecv.tv_usec = 1000 * 50; /* value is microseconds, so timeout set to 50 ms */
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tvForSockRecv, sizeof(tvForSockRecv));

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
  while (gotSigTerm == 0) 
  {
    n = recvfrom(sockfd, requestBuffer, TimeRequestPacketSize, 0, (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
    {
      if(errno == EAGAIN || errno == EINTR) // timeout of the recv operation
      {
        continue;
      }
      else
      {
        syslog(LOG_ERR, "recv from socket failed because: '%m'");
        exit(EXIT_FAILURE);
      }
    }

    if(n < TimeRequestPacketSize)
    {
      // packet is too short - just ignore it (todo: write to log)
      continue;
    }

    // check header of packet - to make sure it is a TSP (time sync protocol) packet
    if(*(requestBuffer + 0) != 'T' ||
        *(requestBuffer + 1) != 'S' ||
        *(requestBuffer + 2) != 'P')
    {
      // not an TSP message (todo: write to log)
      continue;
    }

    struct timeval tvCurrTime;
    gettimeofday(&tvCurrTime, NULL);
    // convert sec to ms and usec to ms
    uint64_t currTimeMsSinceEpoch = ((uint64_t)(tvCurrTime.tv_sec)) * 1000 + ((uint64_t)(tvCurrTime.tv_usec)) / 1000;

    memcpy(replyBuffer, requestBuffer, TimeRequestPacketSize);
    ((TimeReply *)replyBuffer)->timeSinceEphoc1970Ms = currTimeMsSinceEpoch;
    n = sendto(sockfd, replyBuffer, TimeReplyPacketSize, MSG_CONFIRM, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
  }

  close(sockfd);
	syslog(LOG_INFO, "Stopped time sync server daemon '%s'", appName);

}