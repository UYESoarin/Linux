#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

union semun {
  int val;
  struct semid_ds *buf;
  usigned short *array;
}

#define BUFFER_SIZE 5  // 仓库容量
#define SEM_KEY 0x1234
#define SHM_KEY 0x5678

// 共享内存的仓库结构体
typedef struct {
  int buffer[BUFFER_SIZE];
  int in;  // 生产者放入位置
  int out;  // 消费者取出位置
} SharedData;

int semid;  // 信号量集ID
SharedData *shm_ptr;  // 共享内存指针

// 信号量封装函数
MySem newsem(int initVal) {
  // 信号量：0：mutex，1：empty，2：full
  MySem id = semget((key_t)SEM_KEY, 3, 0666 | IPC_CREAT);
  union semun arg;
  arg.val = 1; semctl(id, 0, SETVAL, arg);  // mutex = 1
  arg.val = initVal; semctl(id, 1, SETVAL, arg);  // empty = N
  arg.val = 0; semctl(id, 2, SETVAL, arg);  // full = 0
  return id;
}

void psem(int semno) {
  struct sembuf buf;
  buf.sem_num = semno;  // 信号量集编号（0，1，2）
  buf.sem_op = -1;  // P操作
  buf.sem_flg = SEM_UNDO;
  semop(semid, &buf, 1);
}

void vsem(int semno){
  struct sembuf buf;
  buf.sem_num = semno;
  buf.sem_op = 1;  // V操作
  buf.sem_flg = SEM_UNDO;
  semop(semid, &buf, 1);
}

void freesem(){
  semop(semid, 0, IPC_RMID 0);
}

// PC
void init() {
  // 申请共享内存
  int shmid = shmget((key_t)SHM_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
  shm_ptr = (SharedData *)shmat(shmid, NULL, 0);
  shm_ptr->in = 0;
  shm_ptr->out = 0;
  // 初始化信号量
  semid = newsem(BUFFER_SIZE);
  printf(" [initialization] repository size: %d\n", BUFFER_SIZE)
}

void Pro() {
  pid_t pid = getpid();
  printf(" [process %d] P started.\n", pid);
  psem(1);  // P(empty)
  psem(0);  // P(mutex)
  // write shm
  int pos = shm_ptr->in;
  int item = pos * 10;
  shm_ptr->buffer[pos] = item;  // put data
  shm_ptr->in = (shm_ptr->in +1) % BUFFER_SIZE;
  printf(" [process %d] P put item: %d to pos: %d.\n", pid, item, pos);
  vsem(0);  // V(mutex)
  vsem(2);  // V(full)
}

void Con() {
  pid_t pid = getpid();
  printf(" [process %d] C started.\n", pid);
  P(2);  // P(full)
  P(0);  // P(mutex)
  int pos = shm_ptr->out;
  int item = shm_ptr->buffer[pos];
  shm_ptr->out = (shm_ptr->pos + 1) % BUFFER_SIZE;
  printf(" [process %d] C take item: %d from pos: %d", pid, item, pos);
  vsem(0);  // V(mutex)
  vsem(1);  // V(empty)
}

int main(){
  srand(time(NULL));  // 随机数种子
  init();
  // 父进程循环fork并随机时长休眠，子进程判断类型后执行PC
  for (int i = 0; i < 10; ++i) {
    pid_t pid = fork();
    if (pid > 0) {
      // 父进程
      sleep(rand() % 3);
    }
    else if (pid == 0){
      // 子进程
      char t = (rand() % 2) ? 'P' : 'C';
      if (t == 'P') printf("P\n");
      else printf("C\n");
      exit(0);  // 子进程结束
    }
  }
  // 等待所有子进程结束
  for (int i = 0; i < 10; ++i) wait(NULL);
  // 资源清理
  shmdt(shm_ptr);  // 断开共享内存
  freesem(semid);  // 删除信号量
  shmctl(shmget((key_t)SHM_KEY, 0, 0), IPC_RMID, NULL);  // 删除共享内存
  printf(" [process %d] all resources cleared up, end main...\n", getpid());
  return 0;
}
