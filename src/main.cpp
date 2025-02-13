#include <iostream>
#include "../include/threadPool.h"
#include <chrono>
#include <thread>
using uLong = unsigned long long;
class MyTask:public Task
{
    public:
        MyTask(int begin,int end):begin_(begin),end_(end){}
        
        Any run()
        {
            // std::cout<< "begin" <<std::this_thread::get_id()<<std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            // std::cout << "end" << std::this_thread::get_id() << std::endl;
            uLong sum=0;
            for (int i = begin_; i <= end_; i++) {
              sum += i;
            }
            std::cout << "run" << std::this_thread::get_id() << std::endl;
            return sum;
        }
    private:
     int begin_;
     int end_;
};

int main() {
  {
  ThreadPool pool;
  // pool.setMode(PoolMode::MODE_CACHED);
  pool.startThreadpool(2);
  // pool.setTaskQueMaxSize(1024);
  pool.setMaxThreadCount(10);
  // pool.submitTask(std::make_shared<MyTask>());
  // pool.submitTask(std::make_shared<MyTask>());
  // pool.submitTask(std::make_shared<MyTask>());
  // pool.submitTask(std::make_shared<MyTask>());
  // pool.submitTask(std::make_shared<MyTask>());
  // pool.submitTask(std::make_shared<MyTask>());
  // pool.submitTask(std::make_shared<MyTask>());
  // pool.submitTask(std::make_shared<MyTask>());
  // pool.submitTask(std::make_shared<MyTask>());
  Result res1=pool.submitTask(std::make_shared<MyTask>(1, 100000000));
  Result res2=pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
  Result res3=pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
  Result res4=pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
  Result res5=pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
  Result res6=pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
  // uLong sum1 = res1.get().cast_<uLong>();
  // uLong sum2 = res2.get().cast_<uLong>();
  // uLong sum3 = res3.get().cast_<uLong>();
  // uLong sum4 = res4.get().cast_<uLong>();
  // uLong sum5 = res5.get().cast_<uLong>();
  // uLong sum6 = res6.get().cast_<uLong>();
  //Master-Slave线程模型
  //Master分解任务，然后各个Slave线程分配任务
  //等待各个Slave执行任务输出
  //Master合并各个Slave结果
  // std::cout << "sum" << (sum1 + sum2 + sum3+sum4+sum5+sum6) << std::endl;
  std::cout << "线程池退出" << std::endl;
  }
  std::cout << "作用域退出" << std::endl;
  getchar();
}