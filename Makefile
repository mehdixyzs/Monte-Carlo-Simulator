CC= gcc
CFLAGS= -Wall -Wextra -g -fsanitize=address,undefined #add sanitizer detects mem bugs at runtime overflows are common in this type of simulations 
TARGET= PPSE
SRCS= main.c transmitter.c receiver.c 
OBJS= $(SRCS:.c=.o)


transmitter: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJS) $(TARGET)
