#include "BankLib.h"

#define SHM_NAME "/bank_shared_memory"
#define SEM_NAME "/bank_semaphore"
#define SHM_SIZE 2048

typedef struct {
  char message[1000];
}
ShMem;

ShMem * shmem;

sem_t * sem;

char logFile[100];
char bankName[100];

char serverFifo[100];
int fifo;

char clientFifoName[FIFO_NAME_LEN];
int clientFifo;

char logMessage[256];
char temp[256];

int idCount = 0;

void printUsage() {

  const char usage[] = "Usage: ./BankServer BankName ServerFIFOName\n\nEx:\n./BankServer AdaBank ServerFIFOAdaBank\n";

  write(1, usage, strlen(usage));
}

void deposit(Client * client) {

  char buffer[256];
  char clientmessage[1000];

  memset(buffer, 0, sizeof(buffer));
  memset(clientmessage, 0, sizeof(clientmessage));

  strcat(buffer, "Client");
  strcat(clientmessage, "Client");

  memset(temp, 0, sizeof(temp));
  itoa(client -> number, temp);
  strcat(buffer, temp);
  strcat(clientmessage, temp);

  strcat(buffer, " deposited ");

  memset(temp, 0, sizeof(temp));
  itoa(client -> amount, temp);
  strcat(buffer, temp);
  strcat(buffer, " credits… ");

  if (client -> closed == 1) {
    strcat(buffer, "operation not permitted\n");
    strcat(clientmessage, " something went WRONG\n");
  } else {
    strcat(buffer, "updating log\n");

    strcat(clientmessage, " served... ");
    strcat(clientmessage, client -> id);
    strcat(clientmessage, "\n");

    strcat(shmem -> message, client -> id);
    strcat(shmem -> message, " ");
    strcat(shmem -> message, "D");
    strcat(shmem -> message, " ");
    memset(temp, 0, sizeof(temp));
    itoa(client -> amount, temp);
    strcat(shmem -> message, temp);
    strcat(shmem -> message, "\n");
  }

  write(1, buffer, strlen(buffer));

  write(clientFifo, clientmessage, strlen(clientmessage));
}

void withdraw(Client * client) {

  char buffer[256];
  char clientmessage[1000];

  memset(buffer, 0, sizeof(buffer));
  memset(clientmessage, 0, sizeof(clientmessage));

  strcat(buffer, "Client");
  strcat(clientmessage, "Client");

  memset(temp, 0, sizeof(temp));
  itoa(client -> number, temp);
  strcat(buffer, temp);
  strcat(clientmessage, temp);

  strcat(buffer, " withdraws ");

  memset(temp, 0, sizeof(temp));
  itoa(client -> amount, temp);
  strcat(buffer, temp);
  strcat(buffer, " credits… ");

  int fd = open(logFile, O_RDONLY, PERMS);
  if (fd == -1) {
    write(1, "Couldn't open log file in child process\n", strlen("Couldn't open log file in child process\n"));
    return;
  }

  int flag = 0;

  memset(temp, 0, sizeof(temp));
  ssize_t bytesRead;
  while ((bytesRead = read(fd, temp, sizeof(temp))) > 0) {
    for (ssize_t i = 0; i < bytesRead - strlen(client -> id); i++) {
      if (strncmp( & temp[i], client -> id, strlen(client -> id)) == 0) {
        ssize_t j = i + strlen(client -> id);

        while (j < bytesRead && temp[j] != '\n' && temp[j] != ' ') {
          j++;
        }

        char number_str[20];
        ssize_t num_len = 0;
        while (j < bytesRead && temp[j] != '\n' && temp[j] != ' ') {
          number_str[num_len++] = temp[j++];
        }
        number_str[num_len] = '\0';

        int number = atoi(number_str);

        if (number >= client -> amount) {
          flag = 1;
        }

        close(fd);
      }
    }
  }

  close(fd);

  if (client -> closed == 1 || flag == 0) {
    strcat(buffer, "operation not permitted\n");

    strcat(clientmessage, " something went WRONG\n");
  } else {
    strcat(buffer, "updating log\n");

    strcat(clientmessage, " served... ");
    strcat(clientmessage, client -> id);
    strcat(clientmessage, "\n");

    strcat(shmem -> message, client -> id);
    strcat(shmem -> message, " ");
    strcat(shmem -> message, "W");
    strcat(shmem -> message, " ");
    memset(temp, 0, sizeof(temp));
    itoa(client -> amount, temp);
    strcat(shmem -> message, temp);
    strcat(shmem -> message, "\n");

  }

  write(1, buffer, strlen(buffer));

  write(clientFifo, clientmessage, strlen(clientmessage));

}

pid_t Teller(void * func, void * arg_func) {
  pid_t pid = fork();
  if (pid == 0) {
    sem_wait(sem);
    ((void( * )(void * )) func)(arg_func);
    sem_post(sem);
    exit(0);
  }
  return pid;
}

int waitTeller(pid_t pid, int * status) {
  return waitpid(pid, status, 0);
}

void signal_handler(int signum) {
  int status;
  pid_t pid;

  write(1, "\nSignal received closing active Tellers\n", strlen("\nSignal received closing active Tellers\n"));

  write(1, "Removing ServerFIFO… Updating log file…\n", strlen("Removing ServerFIFO… Updating log file…\n"));
  write(1, bankName, strlen(bankName));
  write(1, " says \"Bye\"...\n", strlen(" says \"Bye\"...\n"));

  if (close(fifo) == -1) {
    write(1, "serverfifo close error\n", strlen("serverfifo close error\n"));
  }

  if (unlink(serverFifo) == -1) {
    write(1, "FIFO unlinking error", strlen("FIFO unlinking error"));
  }

  sem_close(sem);
  sem_unlink(SEM_NAME);
  munmap(shmem, SHM_SIZE);
  shm_unlink(SHM_NAME);

  while ((pid = waitTeller(-1, & status)) > 0);

  exit(0);
}

int main(int argc, char * argv[]) {

  if (argc == 3) {
    strcpy(bankName, argv[1]);

    strcpy(logFile, bankName);
    strcat(logFile, LOG_EXTENSION);

    strcpy(serverFifo, argv[2]);
  } else {
    printUsage();
    return 1;
  }

  write(1, bankName, strlen(bankName));
  write(1, " is active...\n", strlen(" is active...\n"));

  int log, lock;
  char c;
  char buf[256];
  char clientpid[12];
  int i = 0, counter = 0;
  int shm_fd;

  struct sigaction sa;
  memset( & sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  sigaction(SIGINT, & sa, NULL);

  lock = LOCK_EX;
  log = open(logFile, READ_FLAGS, PERMS);

  if (log == -1) {
    write(1, "No previous logs.. Creating the bank database\n", strlen("No previous logs.. Creating the bank database\n"));
    log = open(logFile, (O_WRONLY | O_CREAT | O_APPEND), PERMS);

    if (log == -1) {
      write(1, "Could not open log file: ", strlen("Could not open log file: "));
      write(1, logFile, strlen(logFile));
      write(1, "\n", strlen("\n"));

      return 1;
    }

    if (flock(log, lock) == -1) {
      if (errno == EWOULDBLOCK) {
        write(1, "Log file: ", strlen("Log file: "));
        write(1, logFile, strlen(logFile));
        write(1, " is already locked.\n", strlen(" is already locked.\n"));

        if (close(log) == -1) {
          write(1, "Could not close log file: ", strlen("Could not close log file: "));
          write(1, logFile, strlen(logFile));
          write(1, "\n", strlen("\n"));
        }
      }
      write(1, "Log file couldn't be locked\n", strlen("Log file couldn't be locked\n"));
      return 1;
    }

    write(log, "# ", strlen("# "));
    write(log, bankName, strlen(bankName));
    write(log, "Log file updated @", strlen("Log file updated @"));
    strcpy(temp, get_timestamp());
    write(log, temp, strlen(temp));
    write(log, "\n\n\n## end of log.\n", strlen("\n\n\n## end of log.\n"));
  }

  if (flock(log, LOCK_UN) == -1) {
    write(1, "Could not unlock log file: ", strlen("Could not unlock log file: "));
    write(1, logFile, strlen(logFile));
    write(1, "\n", strlen("\n"));
  }

  if (close(log) == -1) {
    write(1, "Could not close log file: ", strlen("Could not close log file: "));
    write(1, logFile, strlen(logFile));
    write(1, "\n", strlen("\n"));
  }

  umask(0);

  if (mkfifo(serverFifo, PERMS) == -1 && errno != EEXIST) {
    write(log, "FIFO creating error", strlen("FIFO creating error"));
    return 1;
  }

  int fifo = open(serverFifo, O_RDONLY, PERMS);
  if (fifo == -1) {
    write(log, "FIFO opening error", strlen("FIFO opening error"));
    return 1;
  }

  while (1) {

    write(1, "Waiting for clients @", strlen("Waiting for clients @"));
    write(1, serverFifo, strlen(serverFifo));
    write(1, "...\n", strlen("...\n"));

    memset(buf, '\0', sizeof(buf));

    while (read(fifo, & c, 1) <= 0);

    buf[i++] = c;

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
      write(1, "Failed to open shared memory\n", strlen("Failed to open shared memory\n"));
      return 1;
    }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
      write(1, "Failed to truncate size for shared memory\n", strlen("Failed to truncate size for shared memory\n"));
      return 1;
    }

    shmem = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shmem == MAP_FAILED) {
      write(1, "Failed to map shared memory\n", strlen("Failed to map shared memory\n"));
      return 1;
    }

    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);

    if (sem == SEM_FAILED) {
      write(1, "Sem open error\n", strlen("Sem open error\n"));
      return 1;
    }

    while (read(fifo, & c, 1) > 0) {
      buf[i++] = c;

      if (c == ' ') {
        buf[i - 1] = '\0';
        counter = atoi(buf);
        write(1, " - Received ", strlen(" - Received "));
        write(1, buf, strlen(buf));
        write(1, " clients from PID ", strlen(" clients from PID "));
        i = 0;
      } else if (c == '\n') {
        buf[i - 1] = '\0';
        strcpy(clientpid, buf);
        write(1, buf, strlen(buf));
        write(1, "...\n", strlen("...\n"));
        i = 0;
      }
    }

    strcpy(clientFifoName, CLIENT_FIFO);
    strcat(clientFifoName, clientpid);

    clientFifo = open(clientFifoName, WRITE_FLAGS, PERMS);

    write(clientFifo, bankName, strlen(bankName));
    write(clientFifo, "\n", strlen("\n"));

    Client currentClient;
    int totalBytesRead = 0;
    for (i = 0; i < counter; i++) {
      totalBytesRead = 0;
      char * ptr = (char * ) & currentClient;
      while (totalBytesRead < sizeof(Client)) {
        int bytesRead = read(fifo, ptr + totalBytesRead, sizeof(Client) - totalBytesRead);
        if (bytesRead > 0) {
          totalBytesRead += bytesRead;
        }
      }

      if (totalBytesRead == sizeof(Client)) {
        currentClient.number = i + 1;
        currentClient.closed = 0;
        if (strcmp(currentClient.id, "N") == 0) {
          idCount++;
          memset(currentClient.id, '\0', sizeof(currentClient.id));
          strcpy(currentClient.id, BANK_ID_STARTER);
          memset(temp, '\0', sizeof(temp));
          itoa(idCount, temp);
          strcat(currentClient.id, temp);
        } else if (strncmp(currentClient.id, BANK_ID_STARTER, 7) == 0) {
          if (atoi(currentClient.id + strlen(BANK_ID_STARTER)) > counter || atoi(currentClient.id + strlen(BANK_ID_STARTER)) < 1 || strcmp(currentClient.id, BANK_ID_STARTER) != 0) {
            currentClient.closed = 1;

            memset(currentClient.id, '\0', sizeof(currentClient.id));
            strcpy(currentClient.id, BANK_ID_STARTER);
            memset(temp, '\0', sizeof(temp));
            itoa(idCount, temp);
            strcat(currentClient.id, temp);
            idCount++;
          }
        }
        write(1, " -- Teller PID ", strlen(" -- Teller PID "));
        memset(temp, '\0', sizeof(temp));
        if (currentClient.operation == 0) {
          itoa(Teller(deposit, & currentClient), temp);
        } else if (currentClient.operation == 1) {
          itoa(Teller(withdraw, & currentClient), temp);
        }
        write(1, temp, strlen(temp));
        write(1, " is active serving Client", strlen(" is active serving Client"));
        memset(temp, '\0', sizeof(temp));
        itoa(i + 1, temp);
        write(1, temp, strlen(temp));
        write(1, "...\n", strlen("...\n"));
      }
    }
    write(1, "...\n", strlen("...\n"));

    log = open(logFile, O_RDWR);
    if (log < 0) {
      write(1, "log open error\n", strlen("log open error\n"));
      return 1;
    }

    char log_all[MAX_LOGFILE_LENGTH];
    totalBytesRead = read(log, log_all, sizeof(log_all) - 1);
    if (totalBytesRead <= 0) {
      close(log);
      write(1, "log read error\n", strlen("log read error\n"));
      return 1;
    }
    log_all[totalBytesRead] = '\0';

    char * lines[MAX_CLIENT];
    int line_count = 0;

    char * ptr = log_all;
    lines[line_count++] = ptr;
    while ((ptr = strchr(ptr, '\n')) != NULL) {
      * ptr = '\0';
      ptr++;
      lines[line_count++] = ptr;
    }

    if (strncmp(lines[0], "# Adabank Log file updated @", 27) == 0) {

      strcpy(temp, get_timestamp());
      strncpy(lines[0], temp, strlen(temp));
      lines[0][27 + strlen(temp)] = '\0';
    }

    close(clientFifo);

    write(log, shmem -> message, strlen(shmem -> message));

    sem_close(sem);
    sem_unlink(SEM_NAME);
    munmap(shmem, SHM_SIZE);
    shm_unlink(SHM_NAME);

    close(log);

    /*char *msg = shmem->message;
    while (*msg != '\0') {
        char bank_id[MAX_BANKID_LENGTH];
        int k = 0;
        while (*msg != ' ' && *msg != '\0') {
            bank_id[k++] = *msg++;
        }
        bank_id[k] = '\0';

        if (*msg == ' ') msg++;

        char operation = *msg;
        msg++;

        if (*msg == ' ') msg++;

        int amount = 0;
        while (*msg >= '0' && *msg <= '9') {
            amount = amount * 10 + (*msg - '0');
            msg++;
        }

        if (*msg == '\n') msg++;

        for (int j = 0; j < line_count; j++) {
            if (strncmp(lines[j], "BankID_", 7) == 0 || (lines[j][0] == '#' && strncmp(lines[j] + 1, "BankID_", 7) == 0)) {
                char bankid_in_line[16];
                if (lines[j][0] == '#') {
                    int m = 0;
                    char *p = lines[j] + 1;
                    while (*p != ' ' && *p != '\0') {
                        bankid_in_line[m++] = *p++;
                    }
                    bankid_in_line[m] = '\0';
                } else {
                    int m = 0;
                    char *p = lines[j];
                    while (*p != ' ' && *p != '\0') {
                        bankid_in_line[m++] = *p++;
                    }
                    bankid_in_line[m] = '\0';
                }

                // Bank ID eşleşiyor mu
                int equal = 1;
                char *a = bankid_in_line;
                char *b = bank_id;
                while (*a && *b) {
                    char ca = *a;
                    char cb = *b;
                    if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
                    if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
                    if (ca != cb) {
                        equal = 0;
                        break;
                    }
                    a++;
                    b++;
                }
                if (*a != '\0' || *b != '\0') equal = 0;

                if (equal) {
                    // eşleşti, işleme başla
                    if (lines[j][0] == '#') {
                        if (lines[j][0] != '#') {
            memmove(lines[j] + 1, lines[j], strlen(lines[j]) + 1);
            lines[j][0] = '#';
        }
                    }

                    // en son bakiyeyi bul
                    int last_balance = 0;
                    char *p = strrchr(lines[j], ' ');
                    if (p != NULL) {
                        last_balance = atoi(p + 1);
                    }

                    if (operation == 'D') {
                        last_balance += amount;
                    } else if (operation == 'W') {
                        last_balance -= amount;
                    }

                    // yeni işlemi satıra ekle
                    int len = 0;
                    while (lines[j][len] != '\0') len++;

                    char buffer[32];
                    int pos = 0;

                    buffer[pos++] = ' ';
                    buffer[pos++] = operation;
                    buffer[pos++] = ' ';

                    int temp_amount = amount;
                    if (temp_amount == 0) {
                        buffer[pos++] = '0';
                    } else {
                        char temp_digits[10];
                        int td_pos = 0;
                        while (temp_amount > 0) {
                            temp_digits[td_pos++] = (temp_amount % 10) + '0';
                            temp_amount /= 10;
                        }
                        while (td_pos > 0) {
                            buffer[pos++] = temp_digits[--td_pos];
                        }
                    }
                    buffer[pos++] = ' ';

                    int temp_balance = last_balance;
                    if (temp_balance == 0) {
                        buffer[pos++] = '0';
                    } else {
                        char temp_digits[10];
                        int td_pos = 0;
                        while (temp_balance > 0) {
                            temp_digits[td_pos++] = (temp_balance % 10) + '0';
                            temp_balance /= 10;
                        }
                        while (td_pos > 0) {
                            buffer[pos++] = temp_digits[--td_pos];
                        }
                    }
                    buffer[pos] = '\0';

                    int t = 0;
                    while (buffer[t] != '\0') {
                        lines[j][len++] = buffer[t++];
                    }
                    lines[j][len] = '\0';

                    if (last_balance == 0) {
                        if (lines[j][0] != '#') {
                            int l = 0;
                            while (lines[j][l] != '\0') l++;
                            while (l >= 0) {
                                lines[j][l + 1] = lines[j][l];
                                l--;
                            }
                            lines[j][0] = '#';
                        }
                    }
                }
            }
        }
    }

        ftruncate(log, 0);
        lseek(log, 0, SEEK_SET);

        for (i = 0; i < line_count; i++) {
            write(log, lines[i], strlen(lines[i]));
            write(log, "\n", 1);
        }*/

  }

  write(1, "Removing ServerFIFO… Updating log file…\n", strlen("Removing ServerFIFO… Updating log file…\n"));
  write(1, bankName, strlen(bankName));
  write(1, " says \"Bye\"...", strlen(" says \"Bye\"..."));

  if (close(fifo) == -1) {
    write(1, "serverfifo close error\n", strlen("serverfifo close error\n"));
    return 1;
  }

  if (unlink(serverFifo) == -1) {
    write(1, "FIFO unlinking error", strlen("FIFO unlinking error"));
  }

  return 0;
}
