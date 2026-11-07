#include<sdtio.h>
#include<stdlib.h>
#include<time.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>

int main(){
  srand(time(NULL));  // 随机数种子
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
  return 0;
}
