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
    //�̳߳��������,Ӧ������Сһ��
#define  THREADPOOL_MAX_NUM 16
#define  THREADPOOL_AUTO_GROW
#define  TASK_QUEUE_MAXSIZE 1000
//�̳߳�,�����ύ��κ�������ķ����ʽ����������ִ��,���Ի�ȡִ�з���ֵ
//��ֱ��֧�����Ա����, ֧���ྲ̬��Ա������ȫ�ֺ���,Opteron()������
//�̳߳�֧����ͣ,�ָ�,��ֹ,���,��ѯ�̳߳ص�ǰ״̬
    class threadpool{   
        using Task = std::function<void()>;
        vector<thread> _pool;     //�̳߳�
        vector<HANDLE> threadHandlespool;//�̳߳ؾ��
        queue<Task> _tasks;            //�������
        mutex _lock;                   //ͬ��
        condition_variable _task_cv;   //��������
        atomic<bool> _run{ true };     //�̳߳��Ƿ�ִ��
        atomic<int>  _idlThrNum{ 0 };  //�����߳�����
        int max_thread_num = std::thread::hardware_concurrency();
    public:
        inline threadpool(int size = 4) {
            max_thread_num = size;
            addThread(size);
            //�����̳߳ع����߳�
        }
        inline threadpool(){
            max_thread_num = 4;//��ȡӲ���߳�����һ��
            addThread(max_thread_num);
        }
        inline ~threadpool(){
            _run = false;//��ֹ�̳߳� ��ֹ��ʱ���߳�
            _task_cv.notify_all(); // ���������߳�ִ��
            for (thread& thread : _pool) {
                //thread.detach(); // ���̡߳���������
                if (thread.joinable())thread.join(); // �ȴ���������� ǰ�᣺�߳�һ����ִ����
            }
        }

    public:
        // �ύһ������
        // ����.get()��ȡ����ֵ��ȴ�����ִ����,��ȡ����ֵ
        // �����ַ�������ʵ�ֵ������Ա��
        // һ����ʹ��   bind�� .commit(std::bind(&Dog::sayHello, &dog));
        // һ������   mem_fn�� .commit(std::mem_fn(&Dog::sayHello), this)
        template<class F, class... Args>
        auto commit(F&& f, Args&&... args) ->future<decltype(f(args...))>{
            if (!_run)    // stoped ??
                throw runtime_error("commit on ThreadPool is stopped.");
            using RetType = decltype(f(args...)); // class std::result_of<F(Args...)>::type, ���� f �ķ���ֵ����
            auto task = make_shared<packaged_task<RetType()>>(
                bind(forward<F>(f), forward<Args>(args)...)//forward ������תΪ��ֵ
                ); // �Ѻ�����ڼ�����,���(��)Ϊһ���µ�����
            future<RetType> future = task->get_future();//��ȡ����ķ���ֵ
            {    // ������񵽶���
                lock_guard<mutex> lock{ _lock };//�Ե�ǰ���������  lock_guard �� mutex �� stack ��װ�࣬�����ʱ�� lock()��������ʱ�� unlock()
                if (_tasks.size() < TASK_QUEUE_MAXSIZE) {
                    _tasks.emplace([task] () { // push(Task{...}) �ŵ����к���
                        (*task)();//����Ϊthreadpool::packaged_task<void()>
                    });
                }
            }
#ifdef THREADPOOL_AUTO_GROW
            adjustThreadNum();
#endif // !THREADPOOL_AUTO_GROW
            _task_cv.notify_one();
            return future;
        }

        //�����߳�����
        int idlCount() { return _idlThrNum; }
        //�߳�����
        int thrCount() { return _pool.size(); }
        
        //��̬�����̳߳��ڵ��߳����ȼ�

#ifndef THREADPOOL_AUTO_GROW
    private:
#endif // !THREADPOOL_AUTO_GROW
        //���ָ���������߳�
        void addThread(unsigned short size){
            for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0; --size) {//����size���߳�
               //�����߳�����,�������� Ԥ�������� THREADPOOL_MAX_NUM
                _pool.emplace_back([this] { //�����̺߳���
                    while (_run){
                        Task task; // ��ȡһ����ִ�е� task
                        {
                            // unique_lock ��� lock_guard �ĺô��ǣ�������ʱ unlock() �� lock()
                            unique_lock<mutex> lock{ _lock };
                            _task_cv.wait(lock, [this] {//�ȴ��������� �����߳� �ȴ�commit����
                                return !_run || !_tasks.empty();
                            }); // wait ֱ���� task
                            if (!_run && _tasks.empty())
                                return;
                            task = move(_tasks.front()); // ���Ƚ��ȳ��Ӷ���ȡһ�� task
                            _tasks.pop();//�Ӷ�����ɾ��
                        }
                        _idlThrNum--;
                        task(); // ִ�� task
                        _idlThrNum++;
                    }//lambda���ʽ����
                });
                _idlThrNum++;
                
            }
        }
        void delThread(int size){
            //sanity check
            if (size > _pool.size())
                size = _pool.size();
            //����߳���С�ڵ���1����ɾ��
            if (size <= 1)return;
            //ɾ����ǰ���ڿ���״̬���߳�
            //����̳߳���û�п����̣߳���ɾ��
            if (_idlThrNum <= 0)return;
            for (int i = 0; i < size; ++i){
                //�ж��߳��Ƿ��ڿ���״̬
                _pool.back().detach();//�̷߳���
                _pool.pop_back();//ɾ�����һ���߳�
                _idlThrNum--;
            }   
        }
    public:
        
        float GetLoad() {//��ȡ�̳߳صĸ���
            int total = _pool.size();
            return (total-_idlThrNum)/(float)total;
        }


        //����ִ�к��� ���ִ�й����������� ��ֱ�ӷ���һ���µ�ֵ
        
        //�Զ������߳�����
        void adjustThreadNum(){
            //����п����߳� �������������û������ ������߳�����
            if (_idlThrNum > 1 && _tasks.empty()){
                delThread(1);
            }else if (_pool.size() < max_thread_num && GetLoad() > 0.8){
                addThread(1);
            }    
        }
    };
}

#endif  //
