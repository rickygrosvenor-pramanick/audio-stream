##
# CSC209 Assignment 4
# Audio Streaming -- as
#
# @file
# @version 0.2

FLAGS := -Wall --std=gnu99
PORT := port.mk 
TARGETS := as_server as_client stream_debugger

debug: FLAGS += -ggdb3 -DDEBUG
debug: all

release: FLAGS += -O2 
release: all

all: $(PORT) $(TARGETS)

as_server: as_server.o libas.o
	gcc $(FLAGS) -o $@ $^

as_client: as_client.o libas.o
	gcc $(FLAGS) -o $@ $^

stream_debugger: stream_debugger.c
	gcc $(FLAGS) -o $@ $^

%.o: %.c %.h libas.h
	gcc $(FLAGS) -c $< -o $@

$(PORT):
	@echo "Generating a new default port number in $@"
	@awk 'BEGIN{srand();printf("FLAGS += -DDEFAULT_PORT=%d", 55536*rand()+10000)}' > $(PORT)

.PHONY: all clean debug release
clean:
	rm -f *.o *.bak as_server as_client stream_debugger $(PORT)

include $(PORT)

# end
