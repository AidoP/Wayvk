#!/bin/sh
set -e

for shader in shader/*.frag shader/*.vert; do
	glslc "$shader" -o "${shader}.spv"
done

if [ "$1" = "shader" ]; then
	exit
elif [ "$1" = "release" ]; then
	debug="RELEASE"
	rustpath="target/release"
	rustflags="--release"
else
	debug="DEBUG"
	rustpath="target/debug"
fi

cargo build $rustflags

proto_wl='/usr/share/wayland/wayland.xml'
proto_xdg_shell='/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml'
wayland-scanner private-code $proto_wl src/protocol/wayland.c
wayland-scanner server-header $proto_wl src/protocol/wayland.h

wayland-scanner private-code $proto_xdg_shell src/protocol/xdg_shell.c
wayland-scanner server-header $proto_xdg_shell src/protocol/xdg_shell.h

gcc -std=gnu11 -Wall -Werror\
	"$(if [ $debug = 'DEBUG' ]; then echo '-g'; else echo '-O2'; fi)"\
	src/*.c src/*/*.c "$rustpath/libwayvk.a"\
	-lwayland-server -lvulkan -ludev -linput -lpthread -ldl\
	-o target/wayvk -D$debug
