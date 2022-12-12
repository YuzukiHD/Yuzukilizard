/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************/
/**
 * @file subject.h
 * @brief 订阅者接口头文件
 *
 * 定义发布/订阅模式中，订阅者接口
 *
 * @author id: 826
 * @date 2015-11-5
 *
 * @verbatim
    History:
   @endverbatim
 */
#ifndef _SUBJECT_H
#define _SUBJECT_H

#include "common/observer.h"
#include "common/message.h"
#include "common/app_log.h"

#include <list>
#include <string>
#include <mutex>
#include <algorithm>

//#define DEBUG_NOTIFY

namespace EyeseeLinux {

/**
 * @addtogroup Common
 * @{
 */

/**
 * @addtogroup Observer_Subject 发布订阅
 * @{
 */

class ISubject
{
    public:
        ISubject(){}
        virtual ~ISubject() {
           obs_list_.clear();
        }
        /**
         * @brief 添加订阅者到列表
         * @param obs 订阅者对象指针
         */
        virtual void Attach(IObserver *obs) {
            auto it = std::find(obs_list_.begin(), obs_list_.end(), obs);
            if (it == obs_list_.end()) {
                obs_list_.push_back(obs);
#ifdef DEBUG_NOTIFY
                db_msg("== <%s(%p)> Attach <%s(%p)> count[%d] =="
                        , ClassNameInfo().c_str(), this
                        , obs->ClassNameInfo().c_str(), obs
                        , obs_list_.size());
#endif
            } else {
#ifdef DEBUG_NOTIFY
                db_warn("== <%s(%p)> Duplicated Attach <%s(%p)> count[%d] =="
                        , ClassNameInfo().c_str(), this
                        , obs->ClassNameInfo().c_str(), obs
                        , obs_list_.size());
#else
#endif
            }
        }
        /**
         * @brief 移除订阅者到列表
         * @param obs 订阅者对象指针
         */
        virtual void Detach(IObserver *obs) {
            for (std::list<IObserver*>::iterator iter = obs_list_.begin();
                 iter != obs_list_.end();) {
                if (*iter == obs) {
                    iter = obs_list_.erase(iter);
                } else {
                    iter++;
                }
            }
#ifdef DEBUG_NOTIFY
            db_msg("== <%s(%p)> Detach <%s(%p)> remain[%d] =="
                    , ClassNameInfo().c_str(), this
                    , obs->ClassNameInfo().c_str(), obs
                    , obs_list_.size());
#endif
        }
        /**
         * @brief 发送通知消息
         *
         *  将回调所有订阅者的Update方法
         *
         * @note 这里将不保证会异步处理，
         * 是否异步执行取决于Update中的处理
         *
         * @param msg 通知的消息类型
         */
        virtual void Notify(MSG_TYPE msg, int p_CamID=0, int p_recordId=0) {
            if (obs_list_.empty()) {
                return;
            }
            for (std::list<IObserver*>::iterator iter = obs_list_.begin();
                 iter != obs_list_.end(); iter++) {
#ifdef DEBUG_NOTIFY
                db_msg("== <(%p)> Notify <(%p)> ==", this, (*iter));
                db_msg("== <%s> Notify <%s> msg[%d] =="
                        , ClassNameInfo().c_str()
                        , (*iter)->ClassNameInfo().c_str()
                        , msg);
#endif
                (*iter)->Update(msg,p_CamID, p_recordId);
            }
        }

        virtual std::string ClassNameInfo() = 0;

    protected:
        std::list<IObserver*> obs_list_; /**< 观察者列表 */
}; // class ISubject

template<char... Char_>
class ISubject_d : public ISubject
{
public:
    ISubject_d() : class_name_(nullptr) {
        std::vector<char> vec = {Char_...};
        class_name_ = new std::string(vec.begin(), vec.end());
    }

    virtual ~ISubject_d() {
        if (class_name_)
            delete class_name_;
        class_name_ = nullptr;
    }

    std::string ClassNameInfo() {
        if (class_name_)
            return *class_name_;
        return "unknow";
    }

private:
    std::string *class_name_;
};

} // namespace EyeseeLinux

#endif
