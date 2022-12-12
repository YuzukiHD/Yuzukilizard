/*
 * OnvifInterface.h
 *
 *  Created on: 2017年1月4日
 *      Author: liu
 */

#ifndef SRC_ONVIFINTERFACE_H_
#define SRC_ONVIFINTERFACE_H_

namespace onvif {

struct SystemDataAndTime {

};

class ServiceHandler {
public:
    virtual int getSystemDataAndTime() = 0;

    virtual int getNetWorkInterface() = 0;

    virtual int getMediaProfile() = 0;

    virtual int get
};

}



#endif /* SRC_ONVIFINTERFACE_H_ */
