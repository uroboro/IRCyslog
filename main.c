#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <libgen.h>
#include <syslog.h>
#include <notify.h>
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <arpa/inet.h>

static const char *quitNotification  = "com.ircyslog.quit";
static const char *startNotification = "com.ircyslog.start";
static const char *stopNotification  = "com.ircyslog.stop";
static const char *debugNotification = "com.ircyslog.debug";

int print_message(char mode, const char format[], ...) {
	int r;
	if (isatty(STDIN_FILENO)) {
		va_list args;
		va_start(args, format);
		r = vfprintf(mode ? stderr : stdout, format, args);
		va_end(args);
	} else {
		static char buffer[8<<6];
		memset(buffer, 0, sizeof(buffer));

		va_list args;
		va_start(args, format);
		r = vsprintf(buffer, format, args);
		va_end(args);

		const char *syslogPrefix = "\x1b[1;34m[IRCyslog]\x1b[0m";
		syslog(mode ? LOG_ERR : LOG_WARNING, "%s %s", syslogPrefix, buffer);
	}
	return r;
}

void print_usage(int argc, char **argv) {
	print_message(0, "Usage: %s [OPTION...]\n", basename(argv[0]));
	print_message(0, "  -s, --server  Start the program as the server daemon.\n");
//	print_message(0, "  -q, --quit    Stop the program as the server daemon.\n");
	print_message(0, "  -1, --start   Start the IRC server and IRC bot.\n");
	print_message(0, "  -0, --stop    Stop the IRC server and IRC bot.\n");
//	print_message(0, "  -d, --debug   Show debugging messages.\n");
	print_message(0, "  -h, --help    Show this text.\n");
}

uint32_t post(const char *notification) {
	uint32_t r = notify_post(notification);
	print_message(0, "%sPosted \"%s\".\n", (r)?"not ":"", notification);
	return r;
}

int runDaemonRun(void);

int main(int argc, char **argv, char **envp) {
	if (argc != 2) {
		print_usage(argc, argv);
		return 0;
	}

	//flags
	int server_flag = 0;
	int start_flag = 0;
	int stop_flag = 0;
	int debug_flag = 0;
	int help_flag = 0;

	//process options
	struct option long_options[] = {
		/* Flag options. */
		{"server",		no_argument,		&server_flag,	1},
		{"start",		no_argument,		&start_flag,	1},
		{"stop",		no_argument,		&stop_flag,		1},
		{"debug",		no_argument,		&debug_flag,	1},
		{"help",		no_argument,		&help_flag,		1},
		/* End of options. */
		{0, 0, 0, 0}
	};
	int opt;
	int option_index = 0;
	while ((opt = getopt_long(argc, argv, "s01dh", long_options, &option_index)) != -1) {
		switch (opt) {
		case 's':
			server_flag = 1;
			break;

		case '1':
			start_flag = 1;
			break;

		case '0':
			stop_flag = 1;
			break;

		case 'd':
			debug_flag = 1;
			break;

		case 'h':
			help_flag = 1;
			break;

/*
		default:
			print_usage(argc, argv);
			exit(EXIT_FAILURE);
*/
		}
	}

	if (help_flag) {
		print_usage(argc, argv);
		return 1;
	}

	if (start_flag) {
		return post(startNotification);
	}
	if (stop_flag) {
		return post(stopNotification);
	}
	if (debug_flag) {
		return post(debugNotification);
	}

	if (server_flag) {
		return runDaemonRun();
	}

	return 0;
}

int runDaemonRun(void) {
	// Register for all the notifications
	int fd;
	int quitToken = 0;
	if (notify_register_file_descriptor(quitNotification, &fd, 0, &quitToken) != NOTIFY_STATUS_OK) {
		return 1;
	}
	int startToken = 0;
	if (notify_register_file_descriptor(startNotification, &fd, NOTIFY_REUSE, &startToken) != NOTIFY_STATUS_OK) {
		return 1;
	}
	int stopToken = 0;
	if (notify_register_file_descriptor(stopNotification, &fd, NOTIFY_REUSE, &stopToken) != NOTIFY_STATUS_OK) {
		return 1;
	}
	int debugToken = 0;
	if (notify_register_file_descriptor(debugNotification, &fd, NOTIFY_REUSE, &debugToken) != NOTIFY_STATUS_OK) {
		return 1;
	}

	print_message(0, "Started as %d(%d):%d(%d)\n", getuid(), geteuid(), getgid(), getegid());

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	pid_t ngircdPid = 0;
	int ps;
	char debug = 0;

	char shouldContinue = 1;
	while (shouldContinue) {
		int status = select(fd + 1, &readfds, NULL, NULL, NULL);
		if (status <= 0 || !FD_ISSET(fd, &readfds)) {
			continue;
		}

		int t;
		status = read(fd, &t, sizeof(int));
		if (status < 0) {
			break;
		}
		t = ntohl(t); // notify_register_file_descriptor docs: "The value is sent in network byte order."

		// Value in file descriptor matches token for quit notification
		if (t == quitToken) {
			if (debug) print_message(0, "Quitting\n");
			shouldContinue = 0;
		}

		// Value in file descriptor matches token for start notification
		if (t == startToken) {
			if (debug) print_message(0, "Starting\n");
			// Start ngircd as non-daemon, using custom config file
			const char *args[] = {"ngircd", "-n", "-f", "/Library/Application Support/IRCyslog/ircyslog.conf", NULL};
			if (!ngircdPid) {
				ps = posix_spawn(&ngircdPid, "/usr/sbin/ngircd", NULL, NULL, (char *const *)args, NULL);
				if (!ps) {
					if (debug) print_message(0, "Started ngircd with pid:%d\n", ngircdPid);
				}
			}

			// Start the IRC bot
			// ...

		}

		// Value in file descriptor matches token for stop notification
		// or the quit notification was posted
		if (t == stopToken || !shouldContinue) {
			if (debug) print_message(0, "stopping");

			// Stop the IRC bot
			// ...

			if (!ps) { // If the server is running, kill it
				if (debug) print_message(0, "Calling kill with (%d, %d)\n", ngircdPid, SIGTERM);
				int k = kill(ngircdPid, SIGTERM);
				if (debug) print_message(0, "kill'd with exit status %d\n", k);

				// Wait for the server to finish and purge from the process list
				int st;
				if (debug) print_message(0, "Calling waitpid with (%d,%d)\n", ngircdPid, 0);
				int w = waitpid(ngircdPid, &st, 0);
				if (debug) print_message(0, "waitpid'd with exit status %d, stat_loc %d\n", w, st);
			}
		}

		// Value in file descriptor matches token for debug notification
		if (t == debugToken) {
			debug = !debug;
		}
	}

	// Cancel notification watching
	notify_cancel(quitToken);
	notify_cancel(startToken);
	notify_cancel(stopToken);
	notify_cancel(debugToken);

	return 0;
}
