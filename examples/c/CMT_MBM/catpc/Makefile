CC = gcc
LIBDIR = .
CFLAGS = -I $(LIBDIR) -Wall -O3
LDLIBS = -lpthread

SRCS = utils.c
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

	