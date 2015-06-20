#include <string.h>
#include <unistd.h>
#include <uircclient/uircclient.h>

#include "functions.h"

void *makeIRCBot(char *host, int port, char *password, char *nick, char *user) {
	Client *client = ClientNew();
	CO_Call(client, init, host, port, password, nick, user);
	if (!CO_Call(client, connect)) {
		ClientDelete(client);
		return NULL;
	}

	CO_Call(client, receive);
	sleep(1);
	CO_Call(client, login);

	sleep(1);
	CO_Call(client, send, "JOIN #syslog\n");
	sleep(1);
	CO_Call(client, send, "PRIVMSG #syslog :Hello irc!\n");

	return client;
}

void killIRCBot(void *client) {
	ClientDelete((Client *)client);
}

void botSendMessage(void *ctx, const char *date, const char *device, const char *process, const char *pid, const char *type, const char *log) {
	static void *set = NULL;
	static char *_p = NULL;
	Client *client = (Client *)ctx;
	if (addChannelToSetIfNotContained(&set, process)) {
		CO_Call(client, sendWithFormat, "PRIVMSG #syslog :joining #%s\n", process);
		CO_Call(client, sendWithFormat, "JOIN #%s\n", process);
		CO_Call(client, sendWithFormat, "TOPIC #%s %s\n", process, pid);
	}
	if (process) {
		char *msg = convertMessageFromANSIToIRC(log);
		CO_Call(client, sendWithFormat, "PRIVMSG #%s :%s\n", process, msg);
		free(msg);
		free(_p);
		_p = strdup(process);
	} else {
		char *msg = convertMessageFromANSIToIRC(log);
		CO_Call(client, sendWithFormat, "PRIVMSG #%s :%s\n", _p, msg);
		free(msg);
	}
}