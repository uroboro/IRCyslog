#ifndef BOT_FUNCTIONS_H
#define BOT_FUNCTIONS_H

char *convertMessageFromANSIToIRC(const char *message);

// Returns 1 if the channel was added; 0 if already existed in set
int addChannelToSetIfNotContained(void **set, const char *channel);

#endif /* BOT_FUNCTIONS_H */