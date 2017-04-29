// soad: SOcket Activator/Deactivator
// This is free and unencumbered software released into the public domain.
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define die(...)                        \
	if (1) {                              \
		fprintf(stderr, __VA_ARGS__, NULL); \
		exit(1);                            \
	}
// I know about perror, just want to be able to insert other things

#ifdef CLOCK_MONOTONIC_FAST
#define MY_CLOCK CLOCK_MONOTONIC_FAST
#elif CLOCK_MONOTONIC_COARSE
#define MY_CLOCK CLOCK_MONOTONIC_COARSE
#elif CLOCK_MONOTONIC
#define MY_CLOCK CLOCK_MONOTONIC
#elif CLOCK_REALTIME
#warning "No monotonic POSIX clock"
#define MY_CLOCK CLOCK_REALTIME
#else
#error "No POSIX clock?!"
#endif

static const char *progname = "soad";
static char *socket_path = "./socket";
static unsigned int poll_interval = 5;  // seconds
static int inactivity_interval = 60;    // seconds
static int shutdown_signal = SIGTERM;
static int socket_fd = -1;
static pid_t child_pid = -1;
static struct timespec last_activity = {0, 0};

static void *activity_monitor(__attribute__((unused)) void *_) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(socket_fd, &fds);
	int nready;
	for (;;) {
		if ((nready = select(socket_fd + 1, &fds, NULL, NULL, NULL)) == -1) {
			die("select: %s\n", strerror(errno));
		}
		if (nready > 0) {  // A connection has been opened
			if (clock_gettime(MY_CLOCK, &last_activity) == -1) {
				die("clock_gettime: %s\n", strerror(errno));
			}
		}
		usleep(1000 * 5);
	}
}

static void *killer(__attribute__((unused)) void *_) {
	struct timespec cur_time;
	for (;;) {
		sleep(poll_interval);
		if (clock_gettime(MY_CLOCK, &cur_time) == -1) {
			die("clock_gettime: %s\n", strerror(errno));
		}
		if (child_pid > 0 && difftime(cur_time.tv_sec, last_activity.tv_sec) >= inactivity_interval) {
			kill(child_pid, shutdown_signal);
		}
	}
}

static __attribute__((__noreturn__)) void usage() {
	printf(
	    "Usage: %s [-s <socket>] [-t <time-until-stop (seconds)>] "
	    "[-S <shutdown-signal (e.g. 1 for HUP, 2 for INT, ...)>] command arg1 arg2 ...\n",
	    progname);
	exit(1);
}

int main(int argc, char **argv) {
	progname = argv[0];
	int c;
	static struct option longopts[] = {{"socket", required_argument, NULL, 's'},
	                                   {"time-until-stop", required_argument, NULL, 't'},
	                                   {"shutdown-signal", required_argument, NULL, 'S'},
	                                   {NULL, 0, NULL, 0}};
	while ((c = getopt_long(argc, argv, "s:t:S:?h", longopts, NULL)) != -1) {
		switch (c) {
			case 's':
				socket_path = optarg;
				break;
			case 't':
				inactivity_interval = (int)strtol(optarg, NULL, 10);
				break;
			case 'S':
				shutdown_signal = (int)strtol(optarg, NULL, 10);
				break;
			case '?':
			case 'h':
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 1) {
		usage();
	}

	if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		die("socket: %s\n", strerror(errno));
	}
	struct sockaddr_un socket_addr;
	memset(&socket_addr, 0, sizeof(socket_addr));
	socket_addr.sun_family = AF_UNIX;
	strncpy(socket_addr.sun_path, socket_path, sizeof(socket_addr.sun_path) - 1);
	unlink(socket_path);
	if (bind(socket_fd, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) == -1) {
		die("bind: %s\n", strerror(errno));
	}
	if (listen(socket_fd, 1024) == -1) {
		die("listen: %s\n", strerror(errno));
	}
	pthread_t mon_thr;
	pthread_create(&mon_thr, NULL, activity_monitor, 0);
	pthread_t kill_thr;
	pthread_create(&kill_thr, NULL, killer, 0);
	for (;;) {
		int nready;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(socket_fd, &fds);
		if ((nready = select(socket_fd + 1, &fds, NULL, NULL, NULL)) == -1) {
			die("select: %s\n", strerror(errno));
		}
		if (nready < 1) {
			continue;
		}
		child_pid = fork();
		if (child_pid == -1) {
			die("fork\n");
		}
		if (child_pid <= 0) {  // Child
			char pid_s[32];
			snprintf(pid_s, 32, "%d", getpid());
			setenv("LISTEN_PID", (const char *)&pid_s, 1);
			setenv("LISTEN_FDS", "1", 1);
			setenv("LISTEN_FDNAMES", "soad", 1);
			if (execvp(argv[0], argv) == -1) {
				die("execv: %s\n", strerror(errno));
			}
		} else {  // Parent
			int status;
			waitpid(child_pid, &status, 0);
			child_pid = -1;  // For killer thread
			usleep(1000 * 100);
		}
	}
}
