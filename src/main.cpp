#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
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
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char requestBuffer[TimeRequestPacketSize];
  char replyBuffer[TimeReplyPacketSize];
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

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
   * main loop: wait for a datagram, then echo it
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

    // /* 
    //  * gethostbyaddr: determine who sent the datagram
    //  */
    // hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
	// 		  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    // if (hostp == NULL)
    //   error("ERROR on gethostbyaddr");
    // hostaddrp = inet_ntoa(clientaddr.sin_addr);
    // if (hostaddrp == NULL)
    //   error("ERROR on inet_ntoa\n");
    // printf("server received datagram from %s (%s)\n", 
	//    hostp->h_name, hostaddrp);
    // printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
    
    /* 
     * sendto: echo the input back to the client 
     */

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
}