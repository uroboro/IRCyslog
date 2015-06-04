#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <syslog.h>
#include <notify.h>
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <arpa/inet.h>

static const char *syslogPrefix = "\x1b[1;34m[IRCyslog]\x1b[0m";

static const char *quitNotification  = "com.ircyslog.quit";
static const char *startNotification = "com.ircyslog.start";
static const char *stopNotification  = "com.ircyslog.stop";
static const char *debugNotification = "com.ircyslog.debug";

void print_usage(int argc, char **argv) {
	const char *notifications[] = { NULL, quitNotification, startNotification, stopNotification, debugNotification };
	int nSize = sizeof(notifications)/sizeof(char *);

	syslog(LOG_WARNING, "%s Use 1-%d as arguments to send notifications to the daemon.", syslogPrefix, nSize);
	for (int i = 1; i < nSize; i++) {
		syslog(LOG_WARNING, "%s %d: %s.", syslogPrefix, i, notifications[i]);
	}
}

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

	if (server_flag && (start_flag || stop_flag)) {
		fprintf(stderr, "No files to compare.\n");
		return 1;
	}

	if (start_flag) {
		uint32_t r = notify_post(startNotification);
		fprintf(stdout, "%sposted \"%s\".", (r)?"not ":"", startNotification);
		return 0;
	}
	if (stop_flag) {
		uint32_t r = notify_post(stopNotification);
		fprintf(stdout, "%sposted \"%s\".", (r)?"not ":"", stopNotification);
		return 0;
	}
	if (debug_flag) {
		uint32_t r = notify_post(startNotification);
		fprintf(stdout, "%sposted \"%s\".", (r)?"not ":"", startNotification);
		return 0;
	}

	// Daemon

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

	syslog(LOG_WARNING, "%s started as %d(%d):%d(%d)", syslogPrefix, getuid(), geteuid(), getgid(), getegid());

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
			if (debug) syslog(LOG_WARNING, "%s quitting", syslogPrefix);
			shouldContinue = 0;
		}

		// Value in file descriptor matches token for start notification
		if (t == startToken) {
			if (debug) syslog(LOG_WARNING, "%s starting", syslogPrefix);
			// Start ngircd as non-daemon, using custom config file
			const char *args[] = {"ngircd", "-n", "-f", "/Library/Application Support/IRCyslog/ircyslog.conf", NULL};
			if (!ngircdPid) {
				ps = posix_spawn(&ngircdPid, "/usr/sbin/ngircd", NULL, NULL, (char *const *)args, NULL);
				if (!ps) {
					if (debug) syslog(LOG_WARNING, "%s started ngircd with pid:%d", syslogPrefix, ngircdPid);
				}
			}
		}

		// Value in file descriptor matches token for stop notification
		// or the quit notification was posted
		if (t == stopToken || !shouldContinue) {
			if (debug) syslog(LOG_WARNING, "%s stopping", syslogPrefix);

			if (!ps) { // If the server is running, kill it
				if (debug) syslog(LOG_WARNING, "%s calling kill with (%d, %d)", syslogPrefix, ngircdPid, SIGTERM);
				int k = kill(ngircdPid, SIGTERM);
				if (debug) syslog(LOG_WARNING, "%s kill'd with exit status %d", syslogPrefix, k);

				// Wait for the server to finish and purge from the process list
				int st;
				if (debug) syslog(LOG_WARNING, "%s calling waitpid with (%d,%d)", syslogPrefix, ngircdPid, 0);
				int w = waitpid(ngircdPid, &st, 0);
				if (debug) syslog(LOG_WARNING, "%s waitpid'd with exit status %d, stat_loc %d", syslogPrefix, w, st);
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
