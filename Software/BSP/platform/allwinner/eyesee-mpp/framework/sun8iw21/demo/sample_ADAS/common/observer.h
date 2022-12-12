/*************************************************
Copyright (C), 2015, AllwinnerTech. Co., Ltd.
File name: record_preview_controller.h
Author: yinzh@allwinnertech.com
Version: 0.1
Date: 2015-11-5
Description:
    定义用于实现观察者模式中，观察者接口
History:
*************************************************/
#ifndef _OBSERVER_H
#define _OBSERVER_H

#include "common/message.h"
#include "common/thread.h"
#include "common/template_helper.h"

#include <string>
#include <assert.h>
#include <mutex>

namespace EyeseeLinux {

class IObserver
{
    public:
        IObserver(){ ignore_msg_ = false;}
        virtual ~IObserver(){}
        virtual void Update(MSG_TYPE msg, int p_CamID, int p_recordId) = 0;
        virtual std::string ClassNameInfo() = 0;
	
	protected:
        std::mutex msg_mutex_;
        bool ignore_msg_;
}; // class IObserver

template<char... Char_>
class IObserver_d : public IObserver
{
    public:
        IObserver_d() : class_name_(nullptr) {
            std::vector<char> vec = {Char_...};
            class_name_ = new std::string(vec.begin(), vec.end());
        }

        virtual ~IObserver_d() {
            if (class_name_)
                delete class_name_;
            class_name_ = nullptr;
        }

        std::string ClassNameInfo() {
            if (class_name_)
                return *class_name_;
            return "unknown";
        }

		
	private:
        std::string *class_name_;
}; // class IObserver

/**
 * @brief 异步观察者
 *
 * 当被观察者调用Notify时，继承该类的观察者将以异步的方式在子线程中完成回调处理
 */
template<char... Char_>
class AsyncObserver
        : public IObserver_d<'(', 'a', 's', 'y', 'n', 'c', ')', Char_...>
{
    public:
        /**
         * @brief 子线程回调处理函数指针
         *
         * 子类在继承该类时需通过构造函数传入该类型的函数指针完成初始化
         */
        typedef void* (*HANDLER)(void* context);

        AsyncObserver(HANDLER handler, void *context)
            : handler_(handler)
            , context_(context)
        {
            handler_ = handler;
            pthread_mutex_init(&msg_lock_, NULL);
        };
        virtual ~AsyncObserver() {};
        virtual void Update(MSG_TYPE msg, int p_CamID, int p_recordId) = 0;

    protected:
        pthread_mutex_t msg_lock_;
        MSG_TYPE msg_;

        virtual void HandleMessage(MSG_TYPE msg) {
            pthread_mutex_lock(&msg_lock_);
            msg_ = msg;
            pthread_mutex_unlock(&msg_lock_);

            assert(handler_ != NULL);
            ThreadCreate(&handle_thread_id_, NULL, handler_, context_);
        }

    private:
        HANDLER handler_;
        void *context_;
        pthread_t handle_thread_id_;
};
} // namespace EyeseeLinux

#endif
