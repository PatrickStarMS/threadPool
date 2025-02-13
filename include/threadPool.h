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

//Any:可以接受任意数据类型
//模板代码只能写在头文件
class Any {
    public:
     Any() = default;

     //Any类型接受任意类型
     template <typename T>
     //  Any(T data) :base_(new Derive<T>(data)){};
     Any(T data) : base_(std::make_unique<Derive<T>>(data)){};
     ~Any() = default;
     //禁用左值
     Any(const Any&) = delete;
     Any& operator=(const Any&) = delete;
     //公布右值,不写也行
     Any(Any&&) = default;
     Any& operator=(Any&&) = default;

    //将Any对象中存储的任意数据提取出来
     template <typename T>
     T cast_() {
        //从base_中找到它指向的Derive对象，从它里面取出data成员变量
        //基类指针转成派生类指针 RTTI、
        //智能指针的get方法可以获取对应的裸指针
        //这里的T是自己传入的，比对T和前面传入的（get方法获取），是不是一个类型
        Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
        if (pd==nullptr)
        {
          throw "type is unmatch";
        }
        return pd->data_;
     };

    private:
     class Base {
        public:
         virtual ~Base() = default;
        private:
     };
     template<typename T>
     class Derive : public Base {
        public:
         Derive(T data):data_(data){};

         T data_;//保存了其他的任意类型
     };

     private:
      std::unique_ptr<Base> base_;
};

//two modes of ThreadPool:
enum class PoolMode {
    MODE_FIXED, 
    MODE_CACHED, 
};
    
//实现一个信号量
class Semaphore{
public:
//  Semaphore() = default;
 Semaphore(int limit = 0) : resLimit_(limit) {};
 ~Semaphore() = default;
 //PV操作
    //获取一个信号量
 void wait()
 {
    std::unique_lock<std::mutex> lock(mtx_);
    //等待资源
    cond_.wait(lock, [&]() -> bool { return resLimit_ > 0; });
    resLimit_--;//这里不需要再通知了
 }

    //增加一个信号量
 void post() { 
   std::unique_lock<std::mutex> lock(mtx_);
   resLimit_++;
   //类似这与之前的 ，改变条件要通知
   cond_.notify_all();
 }

private:
 int resLimit_;//因为有互斥锁因此可以不用使用原子变量
 std::mutex mtx_;
 std::condition_variable cond_;
};

class Task;//前置声明

//Result类
class Result{
public:
    Result()=default;
    Result(std::shared_ptr<Task> task, bool isValid=true);
    ~Result() = default;
    //setValue()
    //get()
    Any get();
    void setValue(Any any);

   private:
    Any any_;
    Semaphore semaphore_;
    std::shared_ptr<Task> task_;
    std::atomic_bool isValid_;  // 返回值是否有效
};

//need to abstract
//abstract base Task
class Task{
    public:
        //有成员变量了，因此要写构造函数来初始化
     Task();
     ~Task() = default;
     virtual Any run() = 0;  // pure virtual function ()
     void setResult(Result* res);
     void exec();

    private:
     Result* result_;
};
//将线程进行抽象
class Thread{
    public:
    //线程函数对象类型void threadFunc();
     using ThreadFunc = std::function<void(int)>;
     void start();
     
     Thread(ThreadFunc func);
     ~Thread();
    //获取线程id
     int getId()const;

    private:
     ThreadFunc func_;
     static int generateId;//静态成员变量，类外进行初始化
     int threadId_;  // 保存线程id
};

//
class ThreadPool {
    public:
     ThreadPool();
     ~ThreadPool();
     void setMode(PoolMode mode);  // set the mode
     void setTaskQueMaxSize(const int max_tasks);
     void setMaxThreadCount(int max_threads);//cache模式下的最大线程数量
     //  void setInitThreadSize(int init_threadSize);//
     void startThreadpool(int init_threadSize=std::thread::hardware_concurrency());
     Result submitTask(std::shared_ptr<Task> task);
     //ban to use copy and ""=""
     ThreadPool(const Thread&) = delete;
     ThreadPool& operator = (const Thread&) = delete;
    private:
    //  void threadFunc();
     void threadFunc(int threadId);
      //check the running state of pool
     bool checkPoolRunningState() const;


    private:
     PoolMode mode_;//the mode of threadPool
    //  std::vector<std::unique_ptr<Thread>> threads_;                 // vector of threads
     std::unordered_map<int,std::unique_ptr<Thread>> threads_;                 // map of threads
     int init_threadSize_;                      // initial number of threads
     int max_threads_;                       // maximum number of threads
     int curThreadsize_;

     std::queue<std::shared_ptr<Task>> tasksQue_;  // queue of tasks
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

