#include <string.h>
#include <Foundation/NSSet.h>

char *convertMessageFromANSIToIRC(const char *message) {
	return strdup(message);
}

int addChannelToSetIfNotContained(void **_set, const char *channel) {
	NSMutableSet *set = (NSMutableSet *)*_set;
	if (!set) {
		set = [NSMutableSet new];
		*_set = (void *)set;
	}
	if (set && [set containsObject:@(channel)]) {
		return 0;
	}
	[set addObject:@(channel)];
	return 1;
}