export THEOS_PACKAGE_DIR_NAME = packages

T := $(shell ./theossymlinkfix.sh)

include theos/makefiles/common.mk

TOOL_NAME = IRCyslog
IRCyslog_FILES = main.c

include $(THEOS_MAKE_PATH)/tool.mk

after-install::
	@exec ssh -p $(THEOS_DEVICE_PORT) root@$(THEOS_DEVICE_IP) "launchctl unload /Library/LaunchDaemons/com.uroboro.daemon.ircyslog.plist"
	@exec ssh -p $(THEOS_DEVICE_PORT) root@$(THEOS_DEVICE_IP) "launchctl load /Library/LaunchDaemons/com.uroboro.daemon.ircyslog.plist"
