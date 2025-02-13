#include "../include/threadPool.h"
#include <functional>
#include <thread>
#include <iostream>
const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 3;
// Constructor  , variables need to be initialized
ThreadPool::ThreadPool() :
 init_threadSize_(0),
 max_threads_(200),
 max_tasks_(TASK_MAX_THRESHHOLD), 
 taskSize_(),
 mode_(PoolMode::MODE_FIXED),
 isPoolRunning(false),
 idleThreadSize(0),
 curThreadsize_(0)
{};

ThreadPool::~ThreadPool()
{
    //due to we do nothing in constructor, destructor is empty ,but we need to define it to avoid compiler warning.
    isPoolRunning = false;

    notEmpty.notify_all();
    // 等待线程里面所有的线程返回，有两种状态：阻塞&正在执行任务
    //最终，是任务驱动线程，当任务列表没有任务的时候，就是线程都阻塞住了，等待在notEmpty条件变量上
    //因此，要先将这些线程唤醒，再执行操作
    //
    std::cout <<"开始析构" << std::endl;
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    std::cout << "主线程获取锁" << std::endl;
    exitCond_.wait(lock, [&]() -> bool { return threads_.size() == 0; });
    std::cout <<"成功析构" << std::endl;
};

void ThreadPool::setMode(PoolMode mode)
{
    if(checkPoolRunningState()) return;
    this->mode_ = mode;
}  // set the mode
bool ThreadPool::checkPoolRunningState() const { return isPoolRunning; }
void ThreadPool::setTaskQueMaxSize(int max_tasks) { max_tasks_ = max_tasks; }
//设置最大线程数量cache下使用
void ThreadPool::setMaxThreadCount(int max_threads){
  //先判断是不是运行着
  if(checkPoolRunningState()) return;
  if(mode_==PoolMode::MODE_CACHED)
  {
    max_threads_ = max_threads;
  }

}
// void ThreadPool::setInitThreadSize(int init_threadSize){
//   init_threadSize_ = init_threadSize;
// }

void ThreadPool::startThreadpool(int init_threadSize){

  //设置线程池的运行状态
  isPoolRunning = true;
  init_threadSize_ = init_threadSize;
  curThreadsize_ = init_threadSize;
  // now need to init vector (because pool need the vector which has default
  // number )
  for (int i = 0; i < init_threadSize_;++i)
  {
    //将线程池的任务处理函数（线程函数）传给线程对象，然后再去写thread对象的构造函数
    // threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));

    // std::unique_ptr<Thread> ptr =
    //     std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));

    // auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this));
    auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
    int threadId = ptr->getId();
    threads_.emplace(threadId, std::move(ptr));
  }
  //start every thread
  for (int i = 0; i < init_threadSize_;++i)//正好和threadId对应上了
  {
    threads_[i]->start();
    idleThreadSize++;//记录空闲线程
    curThreadsize_++;
  }
}


//线程函数
void ThreadPool::threadFunc(int threadId)
{ 
    //debug
    // std::cout<< "begin" <<std::this_thread::get_id()<<std::endl;
    // std::cout<< "end" <<std::this_thread::get_id()<<std::endl;
    auto lastTime = std::chrono::high_resolution_clock().now();
    // while(isPoolRunning)// for (;;)
    for (;;) 
    {
      std::shared_ptr<Task> task;
      {
        std::unique_lock lock(taskQueMtx_);
        // 
        std::cout <<"tid:   "<<std::this_thread::get_id()<< " attempt to get task" << std::endl;
        //cached模式下，有可能已经创建了很多的线程，但是空闲的超过60s，就应该将多余的线程回收
        //只回收多创建出来的
        //当前时间-上一次执行的时间>60s
        //这里的任务是销毁空闲线程，因此任务队列应该为0
        

          //每一秒钟返回一次，怎么区分：超时返回“还是任务执行返回
          //唤醒有两种，一种是真唤醒，那么此时是有任务的，因此可以跳出循环
          //另外一种是等待一秒没有任务，就唤醒，然后自己销毁
          // while(tasksQue_.size()==0)
          // while(isPoolRunning&&tasksQue_.size()==0)//这里如果大于0，但是while里面没有消费的，因此是一个死循环
          while(tasksQue_.size()==0)
          { 
            if(!isPoolRunning)
            {
              threads_.erase(threadId);
                    // curThreadsize_--;
                    // idleThreadSize--;
              std::cout << "线程id" << std::this_thread::get_id()<<"exit"
                        << std::endl;
              exitCond_.notify_all();
              std::cout << "线程数量" <<threads_.size()<< std::endl;
              return;
            }

            if(mode_==PoolMode::MODE_CACHED){
            // 条件变量，超时返回
            //超时唤醒 (wait_for) 确实不会产生虚假唤醒
            if(std::cv_status::timeout==notEmpty.wait_for(lock, std::chrono::seconds(1)))
            {
              
              auto now = std::chrono::high_resolution_clock().now();
              //转成秒计时
              auto dur = std::chrono::duration_cast<std::chrono::seconds>(
                  now - lastTime);
                  if(dur.count()>=THREAD_MAX_IDLE_TIME&&curThreadsize_>init_threadSize_)
                  {
                    
                    //回收当前线程
                    //改变相关的变量值
                    //把线程对象从线程列表中删除，通过threadId
                    threads_.erase(threadId);
                    curThreadsize_--;
                    idleThreadSize--;
                    std::cout << "线程id" << std::this_thread::get_id()<<"exit"
                              << std::endl;
                    
                    return;
                  }
            }
           
          }
           else{
                   notEmpty.wait(lock);
            }
      // if(!isPoolRunning)
      //  {
      //   threads_.erase(threadId);
      //   // curThreadsize_--;
      //   // idleThreadSize--;
      //   std::cout << "线程id" << std::this_thread::get_id()<<"exit"
      //             << std::endl;
      //   exitCond_.notify_all();
      //   std::cout << "线程数量" <<threads_.size()<< std::endl;
    
      //  }

      }
        //线程池要结束，进行线程回收
      //  if(!isPoolRunning)
      //  {
      //    break;
      //  }

        // 取任务并处理任务;一出现指针就要想是不是需要智能指针
        task = tasksQue_.front();
        tasksQue_.pop();
        taskSize_--;
        idleThreadSize--;//取到任务，线程就不空闲了
        std::cout << "tid:   " << std::this_thread::get_id()
                  << "success to get task" << std::endl;
        //如果还有剩余任务，同通知队友，可以看成面试，一个个的进去面试，前一个面完通知下一个，到时间了（老师完成开始面试），老师会喊
        if(!tasksQue_.empty())
        {
            notEmpty.notify_all();

        }
       
        //通知继续提交生产任务
        notFull.notify_all();
        // 解锁
      }
      
      if (task!=nullptr) {
        task->exec();
      }
      
      idleThreadSize++;//任务处理完成，就空闲了
      lastTime = std::chrono::high_resolution_clock().now();//更新线程执行完任务需要的时间
    }
    
}

//Thread
//starting a thread needs to thread function(Check and process tasks, with different methods of handling them)
int Thread::generateId = 0;
Thread::Thread(ThreadFunc func):func_(func),threadId_(generateId++){}
Thread::~Thread(){}
void Thread::start()
{
    //创建线程执行
    std::thread t(func_,threadId_);
    t.detach();//分离线程
}
int Thread::getId() const { return threadId_; }

//finally to complete
Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
    //思路同连接池
    //获取锁
    std::unique_lock lock(taskQueMtx_);
    // 满：线程通信，等待任务队列有空余（空是给消费者用的）
    // while(tasksQue_.size()==max_tasks_){
    //   notFull.wait(lock);
    // }
    if(!notFull.wait_for(lock, std::chrono::seconds(1),[&]() -> bool { return tasksQue_.size() <= max_tasks_; }))
    {
        //表示等待1s，条件没有满足返回
        std::cout << "task queue is full ,submit task failed,you can submit it "
                     "again later!"
                  << std::endl;
        return Result(task,false);
    }
    // 如果有有空余，将任务加入
    tasksQue_.emplace(task);
    taskSize_++;
    
    // 因为放了新任务，任务对列肯定不空，notEmpty_通知
    notEmpty.notify_all();

  if(mode_==PoolMode::MODE_CACHED&&taskSize_>idleThreadSize&&curThreadsize_<max_threads_)
  {
    // auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this));
    //线程函数加入参数了，因此多一个参数占位符
    std::cout<<"create new"<<std::endl;
    auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
    // threads_.emplace_back(std::move(ptr));
    int threadId = ptr->getId();
    threads_.emplace(threadId, std::move(ptr));
    //启动线程
    threads_[threadId]->start();
    //修改线程个数相关变量
    curThreadsize_++;
    idleThreadSize++;
  }
    return Result(task);

}

Result::Result(std::shared_ptr<Task> task,bool isValid):task_(task),isValid_(isValid)
{
  task_->setResult(this);
}
Any Result::get(){
  if(!isValid_)
  {
    return "";
  }
  semaphore_.wait();//等待任务执行完
  return std::move(any_);
}
void Result::setValue(Any any) {
   any_ = std::move(any);
   semaphore_.post();
}

//TAsk方法实现
Task::Task():result_(nullptr){}
void Task::setResult(Result* res) { result_ = res; }
void Task::exec() 
{ 
  if(result_!=nullptr)
  {
  result_->setValue(run());
  }
}
