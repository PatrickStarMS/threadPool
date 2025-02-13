#ifndef F90141E8_58A0_4E59_9A56_458287FA561F
#define F90141E8_58A0_4E59_9A56_458287FA561F

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>
#include <functional>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 3;

//Any:可以接受任意数据类型
//模板代码只能写在头文件

//two modes of ThreadPool:
enum class PoolMode {
    MODE_FIXED, 
    MODE_CACHED, 
};
    

//将线程进行抽象
class Thread{
    public:
    //线程函数对象类型void threadFunc();
     using ThreadFunc = std::function<void(int)>;
     void start(){
    //创建线程执行
    std::thread t(func_,threadId_);
    t.detach();//分离线程
    }
     
     Thread(ThreadFunc func):func_(func),threadId_(generateId++){}
     
     ~Thread()=default;
    //获取线程id
     int getId()const { return threadId_; }

    private:
     ThreadFunc func_;
     static int generateId;//静态成员变量，类外进行初始化
     int threadId_;  // 保存线程id
};
int Thread::generateId = 0;

//
class ThreadPool {
    public:
     ThreadPool():
        init_threadSize_(0),
        max_threads_(200),
        max_tasks_(TASK_MAX_THRESHHOLD), 
        taskSize_(0),
        mode_(PoolMode::MODE_FIXED),
        isPoolRunning(false),
        idleThreadSize(0),
        curThreadsize_(0)
        {}

     ~ThreadPool(){
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
     }

     void setMode(PoolMode mode){ 
        if(checkPoolRunningState()) return;
        this->mode_ = mode;
        }  // set the mode

     void setTaskQueMaxSize(const int max_tasks) { max_tasks_ = max_tasks; }
     void setMaxThreadCount(int max_threads)//cache模式下的最大线程数量
     {//先判断是不是运行着
        if(checkPoolRunningState()) return;
        if(mode_==PoolMode::MODE_CACHED)
        {
            max_threads_ = max_threads;
        }
    }
     //  void setInitThreadSize(int init_threadSize);//
     void startThreadpool(int init_threadSize=std::thread::hardware_concurrency())
     {

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
    //使用可变惨模板编程，让submitTask可以接受任一任务函数和任意数量的参数
    //  Result submitTask(std::shared_ptr<Task> task);
    template<typename Func,typename... Args>
    //引用折叠
    auto submitTask(Func&& func,Args&&... args)->std::future<decltype(func(args...))>
    {
         //打包任务，放入任务队列
         //现在是将函数作为一个任务，现在将函数抽象成函数对象
         using RType = decltype(func(args...));
         //智能指针管理任务延长生命周期，因为任务不应该是一个局部的
         //由于任务的返回值不一样，使用packaged_task打包实现统一管理,且利用其get方法获取返回值
         //不带参数是因为把参数绑定上面了
         auto task = std::make_shared<std::packaged_task<RType()>>(
          std::bind(std::forward<Func>(func),std::forward<Args>(args)...));
         std::future<RType> result = task->get_future();
         
         // 思路同连接池
         // 获取锁
         std::unique_lock lock(taskQueMtx_);
         // 满：线程通信，等待任务队列有空余（空是给消费者用的）
         // while(tasksQue_.size()==max_tasks_){
         //   notFull.wait(lock);
         // }
         // 任务提交失败情况
         if (!notFull.wait_for(lock, std::chrono::seconds(1), [&]() -> bool {
               return tasksQue_.size() <= max_tasks_;
             })) {
           // 表示等待1s，条件没有满足返回
           std::cout
               << "task queue is full ,submit task failed,you can submit it "
                  "again later!"
               << std::endl;
           // 任务提交失败，返回0值
           //  return Result(task,false);
           auto task = std::make_shared<std::packaged_task<RType()>>(
               []() -> RType { return RType(); });
           // 这里为了统一，设了一个空任务，但也要执行，不然没法get
           // 要解引用，才是执行的函数对象
           (*task)();
           return task->get_future();
                }
            // 如果有有空余，将任务加入
            // tasksQue_.emplace(task);
            // using Task = std::function<void()>;
            //task有返回值类型，using Task = std::function<void()>;需要传入的是没有有返回值类型，
            //包装一下，将task把装成无返回值类型---》task外面套函数
            std::cout << "LINE" << __LINE__ << std::endl;
                tasksQue_.emplace([task]() { (*task)(); });
                taskSize_++;

                // 因为放了新任务，任务对列肯定不空，notEmpty_通知
                notEmpty.notify_all();

                if (mode_ == PoolMode::MODE_CACHED &&
                    taskSize_ > idleThreadSize &&
                    curThreadsize_ < max_threads_) {
                  // auto ptr =
                  // std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this));
                  // 线程函数加入参数了，因此多一个参数占位符
                  std::cout << "create new" << std::endl;
                  auto ptr = std::make_unique<Thread>(std::bind(
                      &ThreadPool::threadFunc, this, std::placeholders::_1));
                  // threads_.emplace_back(std::move(ptr));
                  int threadId = ptr->getId();
                  threads_.emplace(threadId, std::move(ptr));
                  // 启动线程
                  threads_[threadId]->start();
                  // 修改线程个数相关变量
                  curThreadsize_++;
                  idleThreadSize++;
      }
        return result;
    }

     //ban to use copy and ""=""
     ThreadPool(const Thread&) = delete;
     ThreadPool& operator = (const Thread&) = delete;
    private:
    //  void threadFunc();
     void threadFunc(int threadId)
     { 
    //debug
    // std::cout<< "begin" <<std::this_thread::get_id()<<std::endl;
    // std::cout<< "end" <<std::this_thread::get_id()<<std::endl;
    auto lastTime = std::chrono::high_resolution_clock().now();
    // while(isPoolRunning)// for (;;)
    for (;;) 
    {
      Task task;
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
     

      }
        

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
        // task->exec();
        task();
      }

      idleThreadSize++;//任务处理完成，就空闲了
      lastTime = std::chrono::high_resolution_clock().now();//更新线程执行完任务需要的时间
    
    }
    
}

      //check the running state of pool
     bool checkPoolRunningState() const { return isPoolRunning; }


    private:
     PoolMode mode_;//the mode of threadPool
    //  std::vector<std::unique_ptr<Thread>> threads_;                 // vector of threads
     std::unordered_map<int,std::unique_ptr<Thread>> threads_;                 // map of threads
     int init_threadSize_;                      // initial number of threads
     int max_threads_;                       // maximum number of threads
     int curThreadsize_;
    //Task任务==》函数对象，上面可以通过同一个公式推导出自身需要的，这里没法，因为这个类型是外部
    //传进来的，因此需要和外部沟通，没办法直接通话，家中夹层
     using Task = std::function<void()>;
     std::queue<Task> tasksQue_;  // queue of tasks
     std::atomic_int taskSize_;
     int max_tasks_;//任务队列上限

     std::mutex taskQueMtx_;  // make sure the safe of taskQueue
     std::condition_variable
         notFull;  // the queu of tasks is not full---->generator
     std::condition_variable
         notEmpty;  // the queue of tasks is not empty----->consumer
     std::atomic_bool isPoolRunning;
     std::atomic_int idleThreadSize;
     std::condition_variable exitCond_;
};
#endif /* F90141E8_58A0_4E59_9A56_458287FA561F */

