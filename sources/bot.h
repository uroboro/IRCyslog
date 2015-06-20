#ifndef IRCBOT_H
#define IRCBOT_H

void *makeIRCBot(char *host, int port, char *password, char *nick, char *user);

void killIRCBot(void *client);

void botSendMessage(void *ctx, const char *date, const char *device, const char *process, const char *pid, const char *type, const char *log);

#endif /* IRCBOT_H */