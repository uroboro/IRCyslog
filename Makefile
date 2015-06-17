export TARGET = iphone:clang:8.1:6.1
export ARCHS = armv7 arm64

T := $(shell ./theossymlinkfix.sh)

include theos/makefiles/common.mk

TOOL_NAME = IRCyslog
IRCyslog_FILES = $(foreach ext, c cpp m mm x xm xi xmi, $(wildcard sources/*.$(ext)))

include $(THEOS_MAKE_PATH)/tool.mk

after-install::
	@exec ssh -p $(THEOS_DEVICE_PORT) root@$(THEOS_DEVICE_IP) "launchctl unload /Library/LaunchDaemons/com.uroboro.daemon.ircyslog.plist"
	@exec ssh -p $(THEOS_DEVICE_PORT) root@$(THEOS_DEVICE_IP) "launchctl load /Library/LaunchDaemons/com.uroboro.daemon.ircyslog.plist"

remove:
	$(ECHO_NOTHING)exec ssh -p $(THEOS_DEVICE_PORT) root@$(THEOS_DEVICE_IP) "apt-get -y remove $(THEOS_PACKAGE_NAME)"$(ECHO_END)
