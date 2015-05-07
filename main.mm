#include <stdio.h>
#include <notify.h>
#include <spawn.h>

static const char *syslogPrefix = "\x1b[1;34m[IRCyslog]\x1b[0m";

static const char *quitNotification  = "com.ircyslog.quit";
static const char *startNotification = "com.ircyslog.start";
static const char *stopNotification  = "com.ircyslog.stop";
static const char *debugNotification = "com.ircyslog.debug";

int main(int argc, char **argv, char **envp) {
	if (argc > 1) {
		const char *notifications[] = { NULL, quitNotification, startNotification, stopNotification, debugNotification };
		int nSize = sizeof(notifications)/sizeof(char *);
		int n = atoi(argv[1]);
		if (n <= 0 || n >= nSize) { // Print help text
			NSLog(@"%s Use 1-%d as arguments to send notifications to the daemon.", syslogPrefix, nSize);
			for (int i = 1; i < nSize - 1; i++) {
				NSLog(@"%s %d: %s.", syslogPrefix, i, notifications[i]);
			}
			return 0;
		}
		uint32_t r = notify_post(notifications[n]);
		NSLog(@"%s %sposted \"%s\".", syslogPrefix, (r)?"not ":"", notifications[n]);
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

	NSLog(@"%s started as %d(%d):%d(%d)", syslogPrefix, getuid(), geteuid(), getgid(), getegid());

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
			if (debug) NSLog(@"%s quitting", syslogPrefix);
			shouldContinue = 0;
		}

		// Value in file descriptor matches token for start notification
		if (t == startToken) {
			if (debug) NSLog(@"%s starting", syslogPrefix);
			// Start ngircd as non-daemon, using custom config file
			const char *args[] = {"ngircd", "-n", "-f", "/Library/Application Support/IRCyslog/ircyslog.conf", NULL};
			if (!ngircdPid) {
				ps = posix_spawn(&ngircdPid, "/usr/sbin/ngircd", NULL, NULL, (char *const *)args, NULL);
				if (!ps) {
					if (debug) NSLog(@"%s started ngircd with pid:%d", syslogPrefix, ngircdPid);
				}
			}
		}

		// Value in file descriptor matches token for stop notification
		// or the quit notification was posted
		if (t == stopToken || !shouldContinue) {
			if (debug) NSLog(@"%s stopping", syslogPrefix);

			if (!ps) { // If the server is running, kill it
				if (debug) NSLog(@"%s calling kill with (%d, %d)", syslogPrefix, ngircdPid, SIGTERM);
				int k = kill(ngircdPid, SIGTERM);
				if (debug) NSLog(@"%s kill'd with exit status %d", syslogPrefix, k);

				// Wait for the server to finish and purge from the process list
				int st;
				if (debug) NSLog(@"%s calling waitpid with (%d,,%d)", syslogPrefix, ngircdPid, 0);
				int w = waitpid(ngircdPid, &st, 0);
				if (debug) NSLog(@"%s waitpid'd with exit status %d, stat_loc %d", syslogPrefix, w, st);
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
