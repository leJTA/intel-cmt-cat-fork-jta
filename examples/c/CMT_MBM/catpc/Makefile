CC = gcc
LIBDIR = ../../../../lib
CFLAGS = -I $(LIBDIR) -I . -pthread -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector -fPIE -D_FORTIFY_SOURCE=2 \
	-Wunreachable-code -Wsign-compare -Wno-endif-labels \
	-g3 -O2
LDFLAGS=-L$(LIBDIR) -pie -z noexecstack -z relro -z now
LDLIBS = -lpthread -lpqos

SRCS = catpc_utils.c catpc_allocator.c catpc_monitor.c
OBJS = $(SRCS:.c=.o)

MASTER = master_daemon
SLAVE = slave_daemon

all : $(MASTER) $(SLAVE)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

dump:
	echo $(SRCS)
	echo $(OBJS)

$(MASTER): $(MASTER).o $(OBJS)
$(SLAVE): $(SLAVE).o $(OBJS)

clean:
	-rm -f $(MASTER) $(SLAVE) ./*.o

	