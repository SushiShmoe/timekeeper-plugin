# This Makefile acts as a launcher for the various build / program scripts.
# The actual build is done using meson!

.PHONY: default all

default:
	@echo "Please specify a target."

clean:
	./scripts/clean.sh

all:
# this target builds two binaries, server and client
# to run the example, run scripts/serial_loopback.sh in background,
# launch the server binary build/server-Linux/comp-templ-srv-bin in background,
# and then run build/server-Linux/comp-templ-cli-bin
	./scripts/build.sh server Linux
	./scripts/build.sh client Linux

csp:
# this target builds server and client but using CSP communication instead of a plain COM port
	./scripts/build.sh server Linux -Denable_csp=true -Denable_lpldgen=true
	./scripts/build.sh client Linux -Denable_csp=true -Denable_lpldgen=true

dbg:
# this target also builds server and client, but with debug symbols included
	./scripts/build.sh server Linux -Doptimization=0 -Ddebug=true
	./scripts/build.sh client Linux -Doptimization=0 -Ddebug=true

csp-dbg:
	./scripts/build.sh server Linux -Denable_csp=true -Denable_lpldgen=true -Doptimization=0 -Ddebug=true
	./scripts/build.sh client Linux -Denable_csp=true -Denable_lpldgen=true -Doptimization=0 -Ddebug=true

test:
# this target builds server and client as libraries, and then uses those to build
# a single combined binary application, which includes test cases
	./scripts/build.sh server Linux
	./scripts/build.sh client Linux
	./scripts/build.sh combined Linux
# now launch the test - ran with --no-fork option
# so that munit doesn't try to execute tests in child processes, which can mess with timing
	./build/combined-Linux/comp-templ-bin --no-fork

csp-test:
# same as test, but using CSP
	./scripts/build.sh server Linux -Denable_csp=true -Denable_lpldgen=true
	./scripts/build.sh client Linux -Denable_csp=true -Denable_lpldgen=true
	./scripts/build.sh combined Linux -Denable_csp=true -Denable_lpldgen=true
	./build/combined-Linux/comp-templ-bin --no-fork

dbg-test:
# same as test, but with debug symbols
	./scripts/build.sh server Linux -Doptimization=0 -Ddebug=true
	./scripts/build.sh client Linux -Doptimization=0 -Ddebug=true
	./scripts/build.sh combined Linux -Doptimization=0 -Ddebug=true
	gdb --args ./build/combined-Linux/driver-templ-combined-bin --no-fork
