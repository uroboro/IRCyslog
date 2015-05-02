#!/bin/sh

find . -name Makefile -exec dirname {} \; | sed 's_$_/theos_' | xargs -I SL bash -c 'platform=$(uname -s)-$(uname -p); _THEOS=/`[[ $platform == Darwin-arm || $platform == Darwin-arm64 ]] && echo var || echo opt`/theos; [[ $(readlink SL) != $_THEOS ]] && { rm -f SL; ln -s $_THEOS SL; }'
