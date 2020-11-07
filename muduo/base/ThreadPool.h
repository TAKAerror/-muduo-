// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADPOOL_H
#define MUDUO_BASE_THREADPOOL_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"
#include "muduo/base/Thread.h"
#include "muduo/base/Types.h"

#include <deque>
#include <vector>

namespace muduo
{

class ThreadPool : noncopyable
{
 public:
  typedef std::function<void ()> Task;//回调函数，绑定工作

  explicit ThreadPool(const string& nameArg = string("ThreadPool"));
  ~ThreadPool();

  // Must be called before start().
  void setMaxQueueSize(int maxSize) { maxQueueSize_ = maxSize; }
  void setThreadInitCallback(const Task& cb)
  { threadInitCallback_ = cb; }

  void start(int numThreads);//创建numThreads个线程，具体见声明
  void stop();

  const string& name() const
  { return name_; }

  size_t queueSize() const;

  // Could block if maxQueueSize > 0
  // Call after stop() will return immediately.
  // There is no move-only version of std::function in C++ as of C++14.
  // So we don't need to overload a const& and an && versions
  // as we do in (Bounded)BlockingQueue.
  // https://stackoverflow.com/a/25408989
  void run(Task f);  //向线程池工作队列中添加工作f，具体见cc

 private:
  bool isFull() const REQUIRES(mutex_);//判断是否满了
  void runInThread();  //pthread_create函数调用的函数，该函数内部通过调用绑定的回调函数来实现客户端想在线程进行的工作，具体见cc
  Task take();//从工作队列取走工作，线程调用

  mutable MutexLock mutex_;
  Condition notEmpty_ GUARDED_BY(mutex_);//信号变量，通知take函数工作队列不不空
  Condition notFull_ GUARDED_BY(mutex_);//通知run函数工作队列不满
  string name_;   
  Task threadInitCallback_;  
  std::vector<std::unique_ptr<muduo::Thread>> threads_; //线程队列
  std::deque<Task> queue_ GUARDED_BY(mutex_);//工作队列
  size_t maxQueueSize_;//工作队列最大数量
  bool running_;//判断是否start了
};

}  // namespace muduo

#endif  // MUDUO_BASE_THREADPOOL_H
