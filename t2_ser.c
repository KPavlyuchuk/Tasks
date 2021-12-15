#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#define INDEX_NUM 3
#define SHM_SZ 64
#define BUF_SZ (SHM_SZ - INDEX_NUM * sizeof(size_t))

typedef struct Semaphores_id {
  int *empty;
  int *mutex;
  int *full;
} Semid;

typedef struct SharedMemory {
  char *ptr;
  size_t *status;
  size_t *s_ind;
  size_t *r_ind;
  char *buf;
} SharedMemory;

static union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
} Arg;

int *MySemOpen(const char *path, short sem_sz) { // Opening semaphore
  int *sem_id = NULL;
  key_t key;
  struct sembuf sop;

  (void)close(open(path, O_WRONLY | O_CREAT, 0));
  key = ftok(path, 1);
  if (key == -1) {
    perror("ftok error");
    exit(1);
  }
  sem_id = malloc(sizeof(int));

  *sem_id = semget(key, 1, IPC_CREAT | 0777);
  if (*sem_id != -1) {
    Arg.val = 0;
    semctl(*sem_id, 0, SETVAL, Arg);
    sop.sem_num = 0;
    sop.sem_op = 0;
    sop.sem_flg = 0;
    semop(*sem_id, &sop, 1);

    Arg.val = sem_sz;
    semctl(*sem_id, 0, SETVAL, Arg);
  } 
  else {
    if (errno == EEXIST) {
      while (1) {
        if ((*sem_id = semget(key, 1, IPC_CREAT | 0666)) == -1) {
          if (errno == ENOENT) {
            sleep(1);
            continue;
          } 
		  else {
            exit(1);
          }
        } 
		else {
          break;
        }
      }
      while (1) {
        struct semid_ds buf;
        Arg.buf = &buf;
        semctl(*sem_id, 0, IPC_CREAT, Arg);
        if (buf.sem_otime == 0) {
          sleep(1);
          continue;
        } 
		else {
          break;
        }
      }
    } 
	else {
      exit(1);
    }
  }
  return sem_id;
}
int MySemClose(int *sem_id) { // Closing semaphore
  free(sem_id);
  return 0;
}
int MySemRemove(const char *path) { // Removing semaphore
  key_t key;
  int sem_id;

  if((key = ftok(path, 1)) == -1) {
    if(errno != ENOENT) {
      exit(1);
    }
  } 
  else {
    if ((sem_id = semget(key, 1, IPC_CREAT | 0666)) == -1) {
		if (errno != ENOENT) {
			exit(1);
		} 
		else {
			semctl(sem_id, 0, IPC_RMID);
		}
    }
  }
  return 0;
}

int MySemPost(const int *sem_id) { // Increasing semaphore value
  struct sembuf V = {0, 1, 0};
  semop(*sem_id, &V, 1);

  return 0;
}
int MySemWait(const int *sem_id) { // Decreasing semaphore value
  struct sembuf P = {0, -1, 0};
  semop(*sem_id, &P, 1);

  return 0;
}

Semid *SemaphoresInit(char *argv[]) {
  Semid *sem = malloc(sizeof(Semid));
  sem->empty = MySemOpen(argv[0], BUF_SZ);
  sem->mutex = MySemOpen(argv[1], 1);
  sem->full = MySemOpen(argv[2], 0);

  if (sem->empty == NULL || sem->mutex == NULL || sem->full == NULL) {
    perror("MySemOpen error");
    exit(1);
  }

  return sem;
}
void SemaphoreRemove(Semid *sem, char *argv[]) {
  MySemClose(sem->empty);
  MySemClose(sem->mutex);
  MySemClose(sem->full);
  MySemRemove(argv[0]);
  MySemRemove(argv[1]);
  MySemRemove(argv[2]);
  free(sem);
}

void *getaddr(const char *path, size_t shm_sz) {
  key_t key = ftok(path, 1);
  if (key == -1) {
    perror("ftok error for");
    exit(1);
  }

  int shmid = shmget(key, shm_sz, IPC_CREAT);
  if (shmid == -1) {
    printf("%s %d, %ld", path, key, shm_sz);
    perror(" shmget error");
    exit(1);
  }

  char *shmptr = (char *)shmat(shmid, NULL, 0);
  if ((size_t)shmptr == -1) {
    perror("shmat error");
    exit(1);
  }

  return shmptr;
}
SharedMemory *SharedMemoryInit(const char *path) {
  SharedMemory *Shmem = malloc(sizeof(SharedMemory));
  Shmem->ptr = (char *)getaddr(path, SHM_SZ);

  Shmem->status = (typeof(Shmem->status))(Shmem->ptr);
  *(Shmem->status) = 1;

  Shmem->s_ind = (typeof(Shmem->s_ind))
          (Shmem->status + sizeof(typeof(Shmem->s_ind)));
  *(Shmem->s_ind) = 0;

  Shmem->r_ind = (typeof(Shmem->s_ind))
          (Shmem->s_ind + sizeof(typeof(Shmem->r_ind)));
  *(Shmem->r_ind) = 0;

  Shmem->buf = Shmem->ptr + INDEX_NUM * sizeof(typeof(Shmem->status));

  return Shmem;
}
void SharedMemoryRemove(SharedMemory *Shmem) {
  shmdt(Shmem->ptr);
  free(Shmem);
}

void send(char *argv[], SharedMemory *Shmem, Semid *sem) {
  // Get file descriptor
  int fin = open(argv[1], O_RDONLY);
  if (fin == -1) {
    perror("open fd error");
    exit(1);
  }

  size_t *status = Shmem->status;
  size_t *snd_pos = Shmem->s_ind;
  size_t read_sz;
  while (*status) {
    MySemWait(sem->empty);
    MySemWait(sem->mutex);

    // Read data from fd and write in shared memory
    read_sz = read(fin, (Shmem->buf) + (*snd_pos), 1);

    if (read_sz == 0) {
      *status = 0;
      //printf("Sender: EOF!\n");
    } 
	else {
      // Count index
      *snd_pos = ((*snd_pos) + read_sz) % (size_t)(BUF_SZ);
    }

    MySemPost(sem->mutex);
    MySemPost(sem->full);
  }
  while (*status == 0);

  close(fin);
}
void receive(char *argv[], SharedMemory *Shmem, Semid *sem) {
  // Get file descriptor
  int fout = fileno(fopen(argv[2], "w"));
  if (fout == -1) {
    perror("open fd error");
    exit(1);
  }

  // Read data from shared memory and write in file
  size_t *status = Shmem->status;
  size_t *rcv_pos = Shmem->r_ind;
  size_t written_sz;
  while (1) {
    MySemWait(sem->full);
    MySemWait(sem->mutex);

    if ((((*status) == 1) + (semctl(*(sem->full), 0, GETVAL)) != 0) <= 0) {
      MySemPost(sem->mutex);
      MySemPost(sem->empty);
      break;
    }
	
    written_sz = write(fout, Shmem->buf+(*rcv_pos), 1);
    if (written_sz == 0) {
      printf("Bad writing in file\n");
      exit(1);
    }
    // Count index
    *rcv_pos = ((*rcv_pos) + 1) % BUF_SZ;

    MySemPost(sem->mutex);
    MySemPost(sem->empty);
  }
  *status = 1;
  //printf("Receiver: EOF!\n");

  close(fout);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Expected two arguments: src.txt and dst.txt paths.\n");
    exit(1);
  }

  SharedMemory *Shmem = SharedMemoryInit(argv[1]);
  Semid *sem = SemaphoresInit(argv);

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork error");
    exit(1);
  }

  if(pid == 0) {
    receive(argv, Shmem, sem);
  } 
  else {
    send(argv, Shmem, sem);
    wait(NULL);
  }

  SemaphoreRemove(sem, argv);
  SharedMemoryRemove(Shmem);

  return 0;
}