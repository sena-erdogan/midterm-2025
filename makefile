CC = gcc
CFLAGS = -Wall -g

SRCS = BankServer.c BankClient.c
OBJS = BankServer.o BankClient.o
TARGETS = BankServer BankClient

all: $(TARGETS)

BankServer: BankServer.c
	$(CC) $(CFLAGS) BankServer.c -o BankServer

BankClient: BankClient.c
	$(CC) $(CFLAGS) BankClient.c -o BankClient

clean:
	rm -f $(TARGETS)
