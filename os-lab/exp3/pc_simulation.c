#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

// 手动定义联合体
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

typedef int MySem;  // 定义信号量类型

// 仓库容量
#define BUFFER_SIZE 5
// 信号量键值
#define SEM_KEY 0x1234
// 共享内存键值
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
    // 信号量：0: mutex, 1: empty, 2: full
    MySem id = semget((key_t)SEM_KEY, 3, 0666 | IPC_CREAT);
    union semun arg;
    
    arg.val = 1; 
    semctl(id, 0, SETVAL, arg); // mutex = 1
    
    arg.val = initVal; 
    semctl(id, 1, SETVAL, arg); // empty = initVal
    
    arg.val = 0; 
    semctl(id, 2, SETVAL, arg); // full = 0
    return id;
}

void psem(MySem semno) {
    struct sembuf buf;
    buf.sem_num = semno;  // 信号量集编号（0, 1, 2）
    buf.sem_op = -1;  // P操作（申请资源）
    buf.sem_flg = SEM_UNDO;
    semop(semid, &buf, 1);
}

void vsem(MySem semno) {
    struct sembuf buf;
    buf.sem_num = semno;
    buf.sem_op = 1;  // V操作（释放资源）
    buf.sem_flg = SEM_UNDO;
    semop(semid, &buf, 1);
}

// 接收 semID 参数
void freesem(MySem semID) {
    semctl(semID, 0, IPC_RMID, 0); // 删除整个信号量集
}

// PC 核心逻辑

void init() {
    int init_count = 0;
    printf("Please enter the initial inventory (0-%d): ", BUFFER_SIZE);
    scanf("%d", &init_count);
    
    // 容错处理
    if(init_count < 0 || init_count > BUFFER_SIZE) {
        printf("[Error] Invalid inventory. Defaulting to 0.\n");
        init_count = 0;
    }

    // 申请并映射共享内存
    int shmid = shmget((key_t)SHM_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
    shm_ptr = (SharedData *)shmat(shmid, NULL, 0);
    
    // 初始化共享内存指针 (如果有初始库存，生产者从后面开始放)
    shm_ptr->out = 0;
    shm_ptr->in = init_count % BUFFER_SIZE;

    // 初始化信号量
    semid = newsem(BUFFER_SIZE);
    
    // 修正初始库存对 empty 和 full 的影响
    union semun arg;
    arg.val = BUFFER_SIZE - init_count; 
    semctl(semid, 1, SETVAL, arg); // empty = 容量 - 已有库存
    arg.val = init_count; 
    semctl(semid, 2, SETVAL, arg); // full = 已有库存

    printf("[Init] Repository size: %d, Initial inventory: %d\n", BUFFER_SIZE, init_count);
}

void Pro() {
    pid_t pid = getpid();
    
    psem(1);  // P(empty) 申请空槽
    psem(0);  // P(mutex) 申请互斥锁
    
    // 写入共享内存
    int pos = shm_ptr->in;
    shm_ptr->buffer[pos] = pos * 10;  // 存放物品数据
    shm_ptr->in = (shm_ptr->in + 1) % BUFFER_SIZE;

    // 输出格式化
    printf("P <%d> put an item to <%d>.\n", pid, pos);
    
    vsem(0);  // V(mutex) 释放互斥锁
    vsem(2);  // V(full)  增加满槽
}

void Con() {
    pid_t pid = getpid();
    
    psem(2);  // P(full)  申请满槽
    psem(0);  // P(mutex) 申请互斥锁
    
    // 读取共享内存
    int pos = shm_ptr->out;
    int item = shm_ptr->buffer[pos];
    shm_ptr->out = (shm_ptr->out + 1) % BUFFER_SIZE;
    
    printf("C <%d> got an item from <%d>.\n", pid, pos);
    
    vsem(0);  // V(mutex) 释放互斥锁
    vsem(1);  // V(empty) 增加空槽
}

int main() {
    srand(time(NULL));  // 随机数种子
    
    init();

    // 父进程循环 fork 并随机时长休眠，子进程判断类型后执行 PC
    for (int i = 0; i < 10; ++i) {
        pid_t pid = fork();
        
        if (pid > 0) {
            // 父进程逻辑
            sleep(rand() % 3); 
        } 
        else if (pid == 0) {
            // 子进程逻辑
            char t = (rand() % 2) ? 'P' : 'C';
            
            printf("%c <%d> started.\n", t, getpid());
            
            // 调用核心函数
            if (t == 'P') Pro();
            else Con();
            
            exit(0);  // 子进程结束
        }
    }

    // 等待所有子进程结束
    for (int i = 0; i < 10; ++i) wait(NULL);
    
    // 资源清理
    shmdt(shm_ptr);  // 断开共享内存
    freesem(semid);  // 删除信号量
    shmctl(shmget((key_t)SHM_KEY, 0, 0), IPC_RMID, NULL);  // 删除共享内存
    
    printf("All resources cleared up, main process exiting.\n");
    return 0;
}
