#pragma once

#include <pthread.h>
namespace EyeseeLinux {

template <typename T>
class Singleton {
    public:
        static T *GetInstance() {
            if (instance_ == NULL) {
                pthread_mutex_lock(&mutex_);
                if (instance_ == NULL) {
                    instance_ = new T();
                }
                pthread_mutex_unlock(&mutex_);
            }
            return instance_;
        }

        static void Destroy() {
            pthread_mutex_lock(&mutex_);
            if (instance_ != NULL) {
                delete instance_;
                instance_ = NULL;
            }
            pthread_mutex_unlock(&mutex_);
        }

    protected:
        Singleton() {
//            pthread_mutex_init(&mutex_, NULL); //note:!!此处不能初始化锁,单例模式下会先执行GetInstance
                                                 //再执行构造函数Singleton,这样会将锁重新释放掉,造成类被构造两次
        }
        virtual ~Singleton() {
            pthread_mutex_destroy(&mutex_);
        }

    private:
        static T *instance_;
        static pthread_mutex_t mutex_;
};

template <typename T>
T *Singleton<T>::instance_ = NULL;

template <typename T>
pthread_mutex_t Singleton<T>::mutex_ = PTHREAD_MUTEX_INITIALIZER;

}
