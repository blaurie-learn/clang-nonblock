#include "linux.cpp"

/*****
 * This is a project used to explor the man documents surrounding poll, epoll and setting fd's to nonblocking with
 * fcntl.
 *
 * The code is gross on purpose and will likely never be refactored.
 *****/
int main(int argv, char **argc) {
	//nonblock_stdin();
	//stdin_poll();
	//stdin_epoll();
	socket_epoll();
}