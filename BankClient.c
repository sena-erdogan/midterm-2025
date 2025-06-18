#include "BankLib.h"

Client * clients[MAX_CLIENT];

char bankName[100];
char serverFifo[100];
char clientFile[100];

char temp[256];

void printUsage() {

  const char usage[] = "Usage: ./BankClient ClientFileName ServerFIFOName\n\nEx:\n./BankClient Client01.file ServerFIFOAdaBank\n";

  write(1, usage, strlen(usage));
}

int main(int argc, char * argv[]) {

  if (argc == 3) {
    strcpy(clientFile, argv[1]);

    strcpy(serverFifo, argv[2]);
  } else {
    printUsage();
    return 1;
  }

  int fifo, client;
  int i = 0, counter = 0, flag = 0;
  char c;
  char buf[100];
  int clientFifo;
  char clientFifoName[FIFO_NAME_LEN];
  char clientpid[12];

  memset(temp, '\0', strlen(temp));
  itoa(getpid(), temp);
  strcpy(clientpid, temp);

  client = open(clientFile, READ_FLAGS);
  if (client == -1) {
    write(1, "Failed to open clientFile\n", strlen("Failed to open clientFile\n"));
    return 1;
  }
  write(1, "Reading Client.file...\n", strlen("Reading Client.file...\n"));

  while (read(client, & c, 1) > 0) {
    buf[i++] = c;

    if (c == ' ') {
      buf[i - 1] = '\0';
      if (flag == 0) {
        if (counter != MAX_CLIENT) {
          clients[counter] = (Client * ) malloc(sizeof(Client));
        } else {
          write(1, "Max client number achieved. Handling clients\n", strlen("Max client number achieved. Handling clients\n"));
        }
        strcpy(clients[counter] -> id, buf);
        flag++;
      } else if (flag == 1) {
        if (strcmp(buf, "deposit") == 0) {
          clients[counter] -> operation = 0;
        } else if (strcmp(buf, "withdraw") == 0) {
          clients[counter] -> operation = 1;
        }
      }
      memset(buf, '\0', strlen(buf));
      i = 0;
    } else if (c == '\n') {
      buf[i - 1] = '\0';
      clients[counter] -> amount = atoi(buf);
      clients[counter] -> closed = 0;
      memset(buf, '\0', strlen(buf));
      flag = 0;
      i = 0;
      counter++;
    }
  }

  if (close(client) == -1) {
    write(1, "ClientFile close error\n", strlen("ClientFile close error\n"));
    return 1;
  }

  itoa(counter, temp);
  write(1, temp, strlen(temp));
  write(1, " clients to connect... Creating clients...\n", strlen(" clients to connect... Creating clients...\n"));

  fifo = open(serverFifo, WRITE_FLAGS, PERMS);
  if (fifo == -1) {
    write(1, "Cannot connect ", strlen("Cannot connect "));
    write(1, serverFifo, strlen(serverFifo));
    write(1, "...\nExiting...\n", strlen("...\nExiting...\n"));
    for (i = 0; i < counter; i++) {
      free(clients[i]);
    }
    return 1;
  }

  strcpy(clientFifoName, CLIENT_FIFO);
  strcat(clientFifoName, clientpid);
  if (mkfifo(clientFifoName, PERMS) == -1 && errno != EEXIST) {
    write(1, "ClientFifo make Error\n", strlen("ClientFifo make Error\n"));
    return 1;
  }

  itoa(counter, temp);
  write(fifo, temp, strlen(temp));
  write(fifo, " ", strlen(" "));
  write(fifo, clientpid, strlen(clientpid));
  write(fifo, "\n", strlen("\n"));

  if (close(fifo) == -1) {
    write(1, "ServerFifo close error\n", strlen("ServerFifo close error\n"));
    return 1;
  }

  strcpy(clientFifoName, CLIENT_FIFO);
  strcat(clientFifoName, clientpid);

  if (mkfifo(clientFifoName, PERMS) == -1 && errno != EEXIST) {
    write(1, "ClientFifo make Error\n", strlen("ClientFifo make Error\n"));
    return 1;
  }

  clientFifo = open(clientFifoName, READ_FLAGS);
  if (clientFifo == -1) {
    write(1, "Failed to open clientFifo\n", strlen("Failed to open clientFifo\n"));
    return 1;
  }

  memset(buf, '\0', sizeof(buf));

  while (read(clientFifo, & c, 1) > 0) {
    buf[i++] = c;

    if (c == '\n') {
      buf[i - 1] = '\0';
      write(1, "Connected to ", strlen("Connected to "));
      write(1, buf, strlen(buf));
      write(1, "...\n", strlen("...\n"));
      i = 0;
      break;
    }
  }

  fifo = open(serverFifo, WRITE_FLAGS, PERMS);
  if (fifo == -1) {
    write(1, "ServerFifo open error\n", strlen("ServerFifo open error\n"));
    return 1;
  }

  for (i = 0; i < counter; i++) {

    if (write(fifo, clients[i], sizeof(Client)) != sizeof(Client)) {
      write(1, "ServerFifo write error\n", strlen("ServerFifo write error\n"));
      return 1;
    }

    write(1, "Client", strlen("Client"));

    memset(temp, '\0', sizeof(temp));
    itoa(i + 1, temp);
    write(1, temp, strlen(temp));
    write(1, " connected...", strlen(" connected..."));

    if (clients[i] -> operation == 0) {
      write(1, "depositing ", strlen("depositing "));
    } else if (clients[i] -> operation == 1) {
      write(1, "withdrawing ", strlen("withdrawing "));
    }

    memset(temp, '\0', sizeof(temp));
    itoa(clients[i] -> amount, temp);
    write(1, temp, strlen(temp));
    write(1, " credits\n", strlen(" credits\n"));

  }

  write(1, "...\n", strlen("...\n"));

  ssize_t bytesRead;
  while ((bytesRead = read(clientFifo, buf, sizeof(buf))) > 0) {
    write(1, buf, bytesRead);
  }

  write(1, "Exiting...\n", strlen("Exiting...\n"));

  for (i = 0; i < counter; i++) {
    free(clients[i]);
  }

  if (close(fifo) == -1) {
    write(1, "serverfifo close error\n", strlen("serverfifo close error\n"));
    return 1;
  }

  if (close(clientFifo) == -1) {
    write(1, "clientfifo close error\n", strlen("clientfifo close error\n"));
    return 1;
  }

  if (unlink(clientFifoName) == -1) {
    write(1, "ClientFifo unlink error\n", strlen("ClientFifo unlink error\n"));
    return 1;
  }

  return 0;
}
