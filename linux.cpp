#include <cstdio>
#include <fcntl.h>				/* functionality to set handles to non-blocking */
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/epoll.h>

//these are the standard file descriptors for linux
#define LINUX_STDIN_FILNO 0
#define LINUX_STDOUT_FILNO 1
#define LINUX_STDERR_FILNO 2

//yikes
void nonblock_stdin() {
	// First call to fcntl sets the bit flags for this file descriptor. The next call
	if (fcntl(LINUX_STDIN_FILNO, F_SETFL, fcntl(LINUX_STDIN_FILNO, F_GETFL) | O_NONBLOCK)) {
		printf("fcntl failed");
		return;
	}

	char buffer[1000] = {};
	while (true) {

		//lets tick every 5 seconds
		sleep(5);

		fgets(buffer, 1000, stdin);

		printf("Buffer contains: %s", buffer);

		if(strcmp(buffer, "quit\n") == 0) {
			break;
		}

	}
}

void stdin_poll() {
	//poll watches an array of file descriptors
	struct pollfd pfds[1];

	//watch the standard in for in events
	pfds[0].fd = LINUX_STDIN_FILNO;
	pfds[0].events = POLLIN;

	char buffer[1000] = {};

	while (true) {
		//poll blocks though...
		//-1 means to block indefinitely until an event happens
		int events = poll(pfds, 1, -1);
		if (events == -1) {
			break;
		}

		//go through the poll events
		for (int i = 0; i < 1; i++) {
			if (pfds[i].revents & POLLIN) {
				fgets(buffer, 1000, stdin);
				printf("Buffer is: %s\n", buffer);
			}
		}

		if (strcmp(buffer, "q\n") == 0) {
			break;
		}
	}
}

#define MAX_EVENTS 30

void stdin_epoll() {


	//epoll is much more efficient than poll is, particularly as the number of fds grows
	//poll becomes CPU intense as the number of pollfds grows

	//there is also epoll_create (long deprecated), but you should use epoll_create1
	//epoll_create1 was added in glibc 2.9
	int epollfd = epoll_create1(0);
	if (epollfd == -1) {
		printf("Failed to create epoll fd.\n");
		return;
	}

	//Edge Triggered Mode: We will receive events when the state of the watched file descriptors change.
	//Level Triggered Mode: We will continue to receive events until the underlying file descriptor is no longer ready.
	// Suppose a socket is registered with epoll and has received data to be read:
	//	epoll_wait will return, signaling there is data to be read and the reader only consumes a part of the data
	//	from the buffer:
	//		If epoll_wait is called again while there is still data to read:
	//		In level triggered mode, epoll_wait will return as long as there is still data to consume
	//		In event trigger mode, epoll_wait will return once new data is added to the socket
	//
	// level triggered is default. Use EPOLLET flag to go to edge triggered mode

	epoll_event evt = {};
	evt.events = EPOLLIN;
	evt.data.fd = LINUX_STDIN_FILNO;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, evt.data.fd, &evt)) {
		printf("Failed to add descriptor to epoll.\n");
		close(epollfd);
		return;
	}

	epoll_event events[MAX_EVENTS];

	char buffer[1000] = {};

	while (true) {
		int evts = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		printf("We have %d events.\n", evts);
		for (int i = 0; i < evts; i++) {
			fgets(buffer, 1000, stdin);
		}

		if (strcmp(buffer, "q\n") == 0) {
			break;
		}
	}

	if (close(epollfd) == -1) {
		printf("Failed to close epoll fd.\n");
		return;
	}
}



void socket_epoll() {
	//create the socket for listening
	int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	//alternatively,
	//int sockfd = socket(PF_INET, SOCK_STREAM|SOCK_NONBLOCK, IPPROTO_TCP);

	//allow the address to be reused
	int reuse = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	//set the socket to non-blocking:
	//the listen socket probably doesn't need to be non-blocking. It'll still create epoll events
	//and the accept should be guaranteed to have a connection to accept
	//fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);


	sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(8080);
	if (bind(sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		shutdown(sockfd, 2);
		close(sockfd);
		printf("failed to bind");
		return;
	}

	if (listen(sockfd, 3) == -1) {
		shutdown(sockfd, 2);
		close(sockfd);
		printf("failed to bind");
		return;
	}


	int epollfd = epoll_create1(0);
	epoll_event evt = {};
	evt.events = EPOLLIN;
	evt.data.fd = sockfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, evt.data.fd, &evt)) {
		shutdown(sockfd, 2);
		close(sockfd);
		printf("Failed to add descriptor to epoll.\n");
		close(epollfd);
		return;
	}

	epoll_event stdin_evt = {};
	stdin_evt.events = EPOLLIN;
	stdin_evt.data.fd = LINUX_STDIN_FILNO;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, stdin_evt.data.fd, &stdin_evt)) {
		shutdown(sockfd, 2);
		close(sockfd);
		printf("Failed to add descriptor to epoll.\n");
		close(epollfd);
		return;
	}

	epoll_event events[MAX_EVENTS];

	printf("Started...\n");
	fflush(stdout);
	char buff[1000] = {};
	while (true) {
		int fds =  epoll_wait(epollfd, events, MAX_EVENTS, 1000);

		for (int i = 0; i < fds; i++) {

			printf("Event from %d\n", events[i].data.fd);
			if (events[i].data.fd == LINUX_STDIN_FILNO) {
				fgets(buff, 1000, stdin);
			}
			else if (events[i].data.fd == sockfd) {
				//listed socket event (probably accept)
			}
			else {
				//other sockets have read events
			}

		}

		printf("Buffer has: %s\n", buff);
		fflush(stdout);
		if (strcmp(buff, "q\n") == 0) {
			break;
		}
	}

	shutdown(sockfd, 2);
	close(sockfd);
	close(epollfd);
}