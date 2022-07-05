#pragma once
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <atomic>

//#include <condition_variable>
//#include <thread>
//#include <functional>
#include <stdexcept>

namespace std
{
    //线程池最大容量,应尽量设小一点
#define  THREADPOOL_MAX_NUM 16
#define  THREADPOOL_AUTO_GROW
#define  TASK_QUEUE_MAXSIZE 1000
//线程池,可以提交变参函数或拉姆达表达式的匿名函数执行,可以获取执行返回值
//不直接支持类成员函数, 支持类静态成员函数或全局函数,Opteron()函数等
//线程池支持暂停,恢复,终止,清空,查询线程池当前状态
    class threadpool{   
        using Task = std::function<void()>;
        vector<thread> _pool;     //线程池
        vector<HANDLE> threadHandlespool;//线程池句柄
        queue<Task> _tasks;            //任务队列
        mutex _lock;                   //同步
        condition_variable _task_cv;   //条件阻塞
        atomic<bool> _run{ true };     //线程池是否执行
        atomic<int>  _idlThrNum{ 0 };  //空闲线程数量
        int max_thread_num = std::thread::hardware_concurrency();
    public:
        inline threadpool(int size = 4) {
            max_thread_num = size;
            addThread(size);
            //启动线程池管理线程
        }
        inline threadpool(){
            max_thread_num = 4;//获取硬件线程数的一半
            addThread(max_thread_num);
        }
        inline ~threadpool(){
            _run = false;//终止线程池 终止定时器线程
            _task_cv.notify_all(); // 唤醒所有线程执行
            for (thread& thread : _pool) {
                //thread.detach(); // 让线程“自生自灭”
                if (thread.joinable())thread.join(); // 等待任务结束， 前提：线程一定会执行完
            }
        }

    public:
        // 提交一个任务
        // 调用.get()获取返回值会等待任务执行完,获取返回值
        // 有两种方法可以实现调用类成员，
        // 一种是使用   bind： .commit(std::bind(&Dog::sayHello, &dog));
        // 一种是用   mem_fn： .commit(std::mem_fn(&Dog::sayHello), this)
        template<class F, class... Args>
        auto commit(F&& f, Args&&... args) ->future<decltype(f(args...))>{
            if (!_run)    // stoped ??
                throw runtime_error("commit on ThreadPool is stopped.");
            using RetType = decltype(f(args...)); // class std::result_of<F(Args...)>::type, 函数 f 的返回值类型
            auto task = make_shared<packaged_task<RetType()>>(
                bind(forward<F>(f), forward<Args>(args)...)//forward 将参数转为右值
                ); // 把函数入口及参数,打包(绑定)为一个新的任务
            future<RetType> future = task->get_future();//获取任务的返回值
            {    // 添加任务到队列
                lock_guard<mutex> lock{ _lock };//对当前块的语句加锁  lock_guard 是 mutex 的 stack 封装类，构造的时候 lock()，析构的时候 unlock()
                if (_tasks.size() < TASK_QUEUE_MAXSIZE) {
                    _tasks.emplace([task] () { // push(Task{...}) 放到队列后面
                        (*task)();//类型为threadpool::packaged_task<void()>
                    });
                }
            }
#ifdef THREADPOOL_AUTO_GROW
            adjustThreadNum();
#endif // !THREADPOOL_AUTO_GROW
            _task_cv.notify_one();
            return future;
        }

        //空闲线程数量
        int idlCount() { return _idlThrNum; }
        //线程数量
        int thrCount() { return _pool.size(); }
        
        //动态调整线程池内的线程优先级

#ifndef THREADPOOL_AUTO_GROW
    private:
#endif // !THREADPOOL_AUTO_GROW
        //添加指定数量的线程
        void addThread(unsigned short size){
            for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0; --size) {//创建size个线程
               //增加线程数量,但不超过 预定义数量 THREADPOOL_MAX_NUM
                _pool.emplace_back([this] { //工作线程函数
                    while (_run){
                        Task task; // 获取一个待执行的 task
                        {
                            // unique_lock 相比 lock_guard 的好处是：可以随时 unlock() 和 lock()
                            unique_lock<mutex> lock{ _lock };
                            _task_cv.wait(lock, [this] {//等待条件变量 阻塞线程 等待commit操作
                                return !_run || !_tasks.empty();
                            }); // wait 直到有 task
                            if (!_run && _tasks.empty())
                                return;
                            task = move(_tasks.front()); // 按先进先出从队列取一个 task
                            _tasks.pop();//从队列中删除
                        }
                        _idlThrNum--;
                        task(); // 执行 task
                        _idlThrNum++;
                    }//lambda表达式结束
                });
                _idlThrNum++;
                
            }
        }
        void delThread(int size){
            //sanity check
            if (size > _pool.size())
                size = _pool.size();
            //如果线程数小于等于1，则不删除
            if (size <= 1)return;
            //删除当前处于空闲状态的线程
            //如果线程池中没有空闲线程，则不删除
            if (_idlThrNum <= 0)return;
            for (int i = 0; i < size; ++i){
                //判断线程是否处于空闲状态
                _pool.back().detach();//线程分离
                _pool.pop_back();//删除最后一个线程
                _idlThrNum--;
            }   
        }
    public:
        
        float GetLoad() {//获取线程池的负载
            int total = _pool.size();
            return (total-_idlThrNum)/(float)total;
        }


        //尝试执行函数 如果执行过程中阻塞了 则直接返回一个猜的值
        
        //自动调整线程数量
        void adjustThreadNum(){
            //如果有空闲线程 并且任务队列中没有任务 则减少线程数量
            if (_idlThrNum > 1 && _tasks.empty()){
                delThread(1);
            }else if (_pool.size() < max_thread_num && GetLoad() > 0.8){
                addThread(1);
            }    
        }
    };
}

#endif  //
