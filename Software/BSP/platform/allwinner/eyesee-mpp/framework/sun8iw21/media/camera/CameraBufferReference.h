/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CameraBufferReference.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/03
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/
#ifndef _CAMERABUFFERREFERENCE_H_
#define _CAMERABUFFERREFERENCE_H_

#include <type_camera.h>

namespace EyeseeLinux {

class CameraBufferReference
{
public:
    CameraBufferReference(){}
    virtual ~CameraBufferReference(){}
    virtual void increaseBufRef(VIDEO_FRAME_BUFFER_S *pbuf) = 0;
    virtual void decreaseBufRef(unsigned int nBufId) = 0;
    virtual void NotifyRenderStart() = 0;
};

}; /* namespace EyeseeLinux */

#endif  /* _CAMERABUFFERREFERENCE_H_ */

