/*
 * Discovery.cpp
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */
#include <utils.h>
#include <string.h>
#include <stdlib.h>
#include "OnvifMethod.h"
#include "XmlBuilder.h"
#include "SoapService.h"
#include "SoapUtils.h"
#include "dev_ctrl_adapter.h"
#include "OnvifConnector.h"
#include "Packbits.h"
#include "base64.h"

#define POS_MULTIPLE 5000.0f

using namespace std;
using namespace onvif;

void calcOffset(float &left, float &top) {
    left = (left + 1) * POS_MULTIPLE;
    top = (1 - top) * POS_MULTIPLE;
}

void revertOffset(float &left, float &top) {
    left = left / POS_MULTIPLE - 1;
    top = 1 - top / POS_MULTIPLE;
}

int onvif::GetOSDs(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);

    const string fakeToken("main");
    findUseHandle(fakeToken, adaptor, h);
    AWTimeOsd timeOsd;
    int ret = adaptor->overlay_ctrl_->GetTimeOsd(h, timeOsd);
    if (ret != 0) {
        timeOsd.osd_enable = 0;
    }

    if (1 == timeOsd.osd_enable) {
        XmlBuilder *osd = new XmlBuilder("trt:OSDs");
        resp.append(osd);

        float x = timeOsd.left;
        float y = timeOsd.top;
        revertOffset(x, y);
        osd->addAttrib("token", "osd_token_time");
        (*osd)["trt:OSDs.tt:VideoSourceConfigurationToken"] = "video_source_token";
        (*osd)["trt:OSDs.tt:Type"] = "Text";
        (*osd)["trt:OSDs.tt:Position.tt:Type"] = "Custom";
        (*osd)["trt:OSDs.tt:Position.tt:Pos"].addAttrib("x", x);
        (*osd)["trt:OSDs.tt:Position.tt:Pos"].addAttrib("y", y);
        (*osd)["trt:OSDs.tt:TextString.tt:Type"] = "DateAndTime";
        (*osd)["trt:OSDs.tt:TextString.tt:DateFormat"] = "DateAndTime";
        (*osd)["trt:OSDs.tt:TextString.tt:TimeFormat"] = "DateAndTime";
        (*osd)["trt:OSDs.tt:TextString.tt:FontSize"] = "DateAndTime";
    }

    AWDeviceOsd deviceOsd;
    int ret2 = adaptor->overlay_ctrl_->GetDeviceNameOsd(h, deviceOsd);
    if (ret2 != 0) {
        deviceOsd.osd_enable = 0;
    }

    if (1 == deviceOsd.osd_enable) {
        XmlBuilder *osd = new XmlBuilder("trt:OSDs");
        resp.append(osd);

        float x = deviceOsd.left;
        float y = deviceOsd.top;
        revertOffset(x, y);
        osd->addAttrib("token", "osd_token_device");
        (*osd)["trt:OSDs.tt:VideoSourceConfigurationToken"] = "video_source_token";
        (*osd)["trt:OSDs.tt:Type"] = "Text";
        (*osd)["trt:OSDs.tt:Position.tt:Type"] = "Custom";
        (*osd)["trt:OSDs.tt:Position.tt:Pos"].addAttrib("x", x);
        (*osd)["trt:OSDs.tt:Position.tt:Pos"].addAttrib("y", y);
        (*osd)["trt:OSDs.tt:TextString.tt:Type"] = "Plain";
        (*osd)["trt:OSDs.tt:TextString.tt:PlainText"] = deviceOsd.device_name;
        (*osd)["trt:OSDs.tt:TextString.tt:FontSize"] = 10;
    }

    return 0;
}

// TODO: token为空该如何回复
int onvif::GetOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    const string &token = req["Envelope.Body.trt:GetOSD.trt:OSDToken"].stringValue();
    LOG("token: %s", token.c_str());


    return 0;
}

int onvif::GetOSDOptions(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);

    resp["trt:OSDOptions.tt:MaximumNumberOfOSDs"].addAttrib("Total", 2);
    resp["trt:OSDOptions.tt:MaximumNumberOfOSDs"].addAttrib("PlainText", 1);
    resp["trt:OSDOptions.tt:MaximumNumberOfOSDs"].addAttrib("DateAndTime", 1);
    resp["trt:OSDOptions.tt:Type"] = "Text";
    resp["trt:OSDOptions.tt:PositionOption"] = "Custom";

    XmlBuilder *type1 = new XmlBuilder("tt:Type", "Plain");
    XmlBuilder *type2 = new XmlBuilder("tt:Type", "DateAndTime");
    resp["trt:OSDOptions.tt:TextOption"].append(type1);
    resp["trt:OSDOptions.tt:TextOption"].append(type2);

    resp["trt:OSDOptions.tt:TextOption.tt:FontSizeRange.tt:Min"] = 1;
    resp["trt:OSDOptions.tt:TextOption.tt:FontSizeRange.tt:Max"] = 5;

    XmlBuilder *dateFormat1 = new XmlBuilder("tt:DateFormat", "yyyy/MM/dd");
    XmlBuilder *dateFormat2 = new XmlBuilder("tt:DateFormat", "MM/dd/yyyy");
    XmlBuilder *dateFormat3 = new XmlBuilder("tt:DateFormat", "yyyy-MM-dd");
    resp["trt:OSDOptions.tt:TextOption"].append(dateFormat1);
    resp["trt:OSDOptions.tt:TextOption"].append(dateFormat2);
    resp["trt:OSDOptions.tt:TextOption"].append(dateFormat3);

    XmlBuilder *timeFormat1 = new XmlBuilder("tt:TimeFormat", "HH:mm:ss");
    XmlBuilder *timeFormat2 = new XmlBuilder("tt:TimeFormat", "hh:mm:ss");
    resp["trt:OSDOptions.tt:TextOption"].append(timeFormat1);
    resp["trt:OSDOptions.tt:TextOption"].append(timeFormat2);

    return 0;
}

int onvif::SetOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);

    XmlBuilder &osd = req["Envelope.Body.trt:SetOSD.trt:OSD"];
    osd["trt:OSD.tt:Type"];
    osd["trt:OSD.tt:Position.tt:Type"];
    float x = atof(osd["trt:OSD.tt:Position.tt:Pos"].attrib("x").c_str());
    float y = atof(osd["trt:OSD.tt:Position.tt:Pos"].attrib("y").c_str());
    calcOffset(x, y);
    LOG("x: %f, y: %f", x, y);

    const string fakeToken("main");
    findUseHandle(fakeToken, adaptor, h);

    const string &type = osd["trt:OSD.tt:TextString.tt:Type"].stringValue();
    if ("Plain" == type) {
        LOG("osd set plain");
        AWDeviceOsd dev;
        adaptor->overlay_ctrl_->GetDeviceNameOsd(h, dev);
        const string &text = osd["trt:OSD.tt:TextString.tt:PlainText"].stringValue();
        int len = OSD_MAX_TEXT_LEN > text.size() ? text.size() : OSD_MAX_TEXT_LEN;
        memset(dev.device_name, 0, OSD_MAX_TEXT_LEN);
        strncpy(dev.device_name, text.c_str(), len);
        dev.left = x;
        dev.top = y;
        adaptor->overlay_ctrl_->SetDeviceNameOsd(h, dev);

    } else if ("DateAndTime" == type) {
        const string &df = osd["trt:OSD.tt:TextString.tt:DateFormat"].stringValue();
        const string &tf = osd["trt:OSD.tt:TextString.tt:TimeFormat"].stringValue();
        DBG("osd set date and time, date format: %s, time format: %s", df.c_str(), tf.c_str());
        AWTimeOsd timeOsd;
        adaptor->overlay_ctrl_->GetTimeOsd(h, timeOsd);
        timeOsd.left = x;
        timeOsd.top = y;
        adaptor->overlay_ctrl_->SetTimeOsd(h, timeOsd);

    } else {
        LOG("unknown type");
    }

    adaptor->overlay_ctrl_->SaveConfig();

    return 0;
}

int onvif::SetOSDs(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);

    return -1;
}

int onvif::CreateOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    do {
        DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);

        XmlBuilder &osd = req["Envelope.Body.trt:CreateOSD.trt:OSD"];
//    osd["trt:OSD.tt:Type"];
//    osd["trt:OSD.tt:Position.tt:Type"];
        float x = atof(osd["trt:OSD.tt:Position.tt:Pos"].attrib("x").c_str());
        float y = atof(osd["trt:OSD.tt:Position.tt:Pos"].attrib("y").c_str());
        calcOffset(x, y);

        const string fakeToken("main");
        findUseHandle(fakeToken, adaptor, h);

        int ret = 0;
        const string &type = osd["trt:OSD.tt:TextString.tt:Type"].stringValue();
        if ("Plain" == type) {
            AWDeviceOsd dev;
            ret = adaptor->overlay_ctrl_->GetDeviceNameOsd(h, dev);
            if (ret < 0) break;
            const string &text = osd["trt:OSD.tt:TextString.tt:PlainText"].stringValue();
            int len = OSD_MAX_TEXT_LEN > text.size() ? text.size() : OSD_MAX_TEXT_LEN;
            memset(dev.device_name, 0, OSD_MAX_TEXT_LEN);
            strncpy(dev.device_name, text.c_str(), len);
            dev.left = x;
            dev.top = y;
            dev.osd_enable = 1;
            ret = adaptor->overlay_ctrl_->SetDeviceNameOsd(h, dev);
            if (ret < 0) break;

        } else if ("DateAndTime" == type) {
            AWTimeOsd timeOsd;
            ret = adaptor->overlay_ctrl_->GetTimeOsd(h, timeOsd);
            if (ret < 0) break;
            timeOsd.left = x;
            timeOsd.top = y;
            timeOsd.osd_enable = 1;
            ret = adaptor->overlay_ctrl_->SetTimeOsd(h, timeOsd);
            if (ret < 0) break;

        } else {
            LOG("unknown type");
        }
        adaptor->overlay_ctrl_->SaveConfig();

        return 0;
    } while (0);

    return -1;
}

int onvif::DeleteOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    do {
        DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);

        const string &osdToken = req["Envelope.Body.trt:DeleteOSD.trt:OSDToken"].stringValue();
        LOG("token: %s", osdToken.c_str());

        findUseHandle(osdToken, adaptor, h);

        if (string::npos != osdToken.find("time")) {
            AWTimeOsd timeOsd;
            int ret = adaptor->overlay_ctrl_->GetTimeOsd(h, timeOsd);
            if (ret < 0) break;
            timeOsd.osd_enable = 0;
            ret = adaptor->overlay_ctrl_->SetTimeOsd(h, timeOsd);
            if (ret < 0) break;
        } else if (string::npos != osdToken.find("device")) {
            AWDeviceOsd devOsd;
            int ret = adaptor->overlay_ctrl_->GetDeviceNameOsd(h, devOsd);
            if (ret < 0) break;
            devOsd.osd_enable = 0;
            ret = adaptor->overlay_ctrl_->SetDeviceNameOsd(h, devOsd);
            if (ret < 0) break;
        } else {
            LOG("unknown osd token! token: %s", osdToken.c_str());
            return -1;
        }

        adaptor->overlay_ctrl_->SaveConfig();

        return 0;
    } while (0);

    return -1;
}

static void mdAreaToActiveCells(AWAlarmMd &alarmMd, string &activeCells) {
    char packData[MOTION_AREA_NUM];
    memset(packData, 0, MOTION_AREA_NUM);
    int packDataSize = tiff6_PackBits((unsigned char*) alarmMd.detec_area, sizeof(alarmMd.detec_area),
            (unsigned char*) packData);
    int base64DataSize = 0;
    bool ret = Base64::Encode(packData, &activeCells);
//    LOG("packBit base64 output %s\n", activeCells.c_str());
}

static void activeCellsToMdArea(const string &activeCells, AWAlarmMd &alarm) {
    string packData;
    Base64::Decode(activeCells, &packData);
    tiff6_unPackBits(packData.data(), packData.size(), (unsigned char*)alarm.detec_area);
}

int onvif::GetRules(const struct OnvifContext* context, XmlBuilder& req, XmlBuilder& resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);
    const string fakeToken("main");
    findUseHandle(fakeToken, adaptor, h);
    AWAlarmMd alarmMd;
    int ret = adaptor->event_ctrl_->GetAlarmMdConfig(h, alarmMd);
    if (ret != 0) {
        alarmMd.md_enable = 0;
    }

//    if (1 == alarmMd.md_enable) {
    if (true) {
        XmlBuilder *rule1 = new XmlBuilder("tan:Rule");
        resp.append(rule1);
        rule1->addAttrib("Name", "AWCellMotionDetector");
        rule1->addAttrib("Type", "tt:CellMotionDetector");

        XmlBuilder &param = rule1->getPath("tt:Parameters");
        XmlBuilder *item1 = new XmlBuilder("tt:SimpleItem");
        XmlBuilder *item2 = new XmlBuilder("tt:SimpleItem");
        XmlBuilder *item3 = new XmlBuilder("tt:SimpleItem");
        XmlBuilder *item4 = new XmlBuilder("tt:SimpleItem");
        param.append(item1);
        param.append(item2);
        param.append(item3);
        param.append(item4);

        item1->addAttrib("Name", "MinCount");
        item1->addAttrib("Value", "5");

        item2->addAttrib("Name", "AlarmOnDelay");
        item2->addAttrib("Value", "1000");

        item3->addAttrib("Name", "AlarmOffDelay");
        item3->addAttrib("Value", "1000");

        string activeCells;
        mdAreaToActiveCells(alarmMd, activeCells);
        item4->addAttrib("Name", "ActiveCells");
        item4->addAttrib("Value", activeCells);
    }

    AWAlarmCover alarmCover;
    ret = adaptor->event_ctrl_->GetAlarmCoverConfig(h, alarmCover);
    if (ret != 0) {
        alarmCover.cover_enable = 0;
    }

//    if (1 == alarmCover.cover_enable) {
    if (true) {
        XmlBuilder *rule = new XmlBuilder("tan:Rule");
        resp.append(rule);
        rule->addAttrib("Name", "AWTamperDetector");
        rule->addAttrib("Type", "tt:TamperDetector");
        rule->getPath("tt:Parameters.tt:ElementItem").addAttrib("Name", "Field");

        XmlBuilder &polygen = rule->getPath("tt:Parameters.tt:ElementItem.tt:PolygonConfiguration.tt:Polygon");
        XmlBuilder *point1 = new XmlBuilder("tt:Point");
        XmlBuilder *point2 = new XmlBuilder("tt:Point");
        XmlBuilder *point3 = new XmlBuilder("tt:Point");
        XmlBuilder *point4 = new XmlBuilder("tt:Point");
        polygen.append(point1);
        polygen.append(point2);
        polygen.append(point3);
        polygen.append(point4);

        point1->addAttrib("x", "0");
        point1->addAttrib("y", "240");

        point2->addAttrib("x", "320");
        point2->addAttrib("y", "240");

        point3->addAttrib("x", "320");
        point3->addAttrib("y", "0");

        point4->addAttrib("x", "0");
        point4->addAttrib("y", "0");
    }

    return 0;
}

int onvif::CreateRules(const struct OnvifContext* context, XmlBuilder& req, XmlBuilder& resp) {
    req["Envelope.Body.tan:CreateRules.tan:ConfigurationToken"].stringValue();
    XmlBuilder &param = req["Envelope.Body.tan:CreateRules.tan:Rule.tt:Parameters"];
    int count = param.childCount();
    for (int i = 0; i < count; i++) {
        XmlBuilder *item = param.getChild(i);
        const string &name = (*item)["tt:SimpleItem"].attrib("Name");
        const string &value = (*item)["tt:SimpleItem"].attrib("Value");
        if ("ActiveCells" == name) {
            DBG("active cells: %s", value.c_str());
        }
    }

    return 0;
}

int onvif::ModifyRules(const struct OnvifContext* context, XmlBuilder& req, XmlBuilder& resp) {
    req["Envelope.Body.tan:ModifyRules.tan:ConfigrationToken"].stringValue();
    req["Envelope.Body.tan:ModifyRules.tan:Rule.tt:Name"].stringValue();
    req["Envelope.Body.tan:ModifyRules.tan:Rule.tt:Type"].stringValue();
    XmlBuilder &params = req["Envelope.Body.tan:ModifyRules.tan:Rule.tt:Parameters"];

    return 0;
}

int onvif::CreateAnalyticsModules(const struct OnvifContext* context, XmlBuilder& req, XmlBuilder& resp) {

    req["Envelope.Body.tan:CreateAnalyticsModules.tan:ConfigurationToken"].stringValue();
    req["Envelope.Body.tan:CreateAnalyticsModules.tan:ConfigurationToken"].stringValue();
    req["Envelope.Body.tan:CreateAnalyticsModules.tan:AnalyticsModule.tt:Name"].stringValue();
    req["Envelope.Body.tan:CreateAnalyticsModules.tan:AnalyticsModule.tt:Type"].stringValue();
    XmlBuilder &params = req["Envelope.Body.tan:CreateAnalyticsModules.tan:AnalyticsModule.tt:Parameters"];

    int count = params.childCount();
    LOG("parameter count: %d", count);

    return 0;
}

int onvif::DeleteAnalyticsModules(const struct OnvifContext* context, XmlBuilder& req, XmlBuilder& resp) {
    req["Envelope.Body.tan:CreateAnalyticsModules.tan:ConfigurationToken"].stringValue();
    req["Envelope.Body.tan:CreateAnalyticsModules.tan:AnalyticsModuleName"].stringValue();

    return 0;
}

int onvif::GetAnalyticsModules(const struct OnvifContext* context, XmlBuilder& req, XmlBuilder& resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);
    const string fakeToken("main");
    findUseHandle(fakeToken, adaptor, h);

    AWAlarmMd alarmMd;
    int ret = adaptor->event_ctrl_->GetAlarmMdConfig(h, alarmMd);
    if (ret != 0) {
        alarmMd.md_enable = 0;
    }

//    if (1 == alarmMd.md_enable) {
    if (true) {
        XmlBuilder *module1 = new XmlBuilder("tan:AnalyticsModule");
        resp.append(module1);
        module1->addAttrib("Name", "AWCellMotion");
        module1->addAttrib("Type", "tt:CellMotionEngine");
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:SimpleItem"].addAttrib("Name", "Sensitivity");
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:SimpleItem"].addAttrib("Value", alarmMd.sensitive);
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:ElementItem"].addAttrib("Name", "Layout");
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:ElementItem.tt:CellLayout"].addAttrib("Columns", "22");
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:ElementItem.tt:CellLayout"].addAttrib("Rows", "18");
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:ElementItem.tt:CellLayout.tt:Transformation.tt:Translate"].addAttrib("x", "1");
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:ElementItem.tt:CellLayout.tt:Transformation.tt:Translate"].addAttrib("y", "-1");
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:ElementItem.tt:CellLayout.tt:Transformation.tt:Scale"].addAttrib("x", "0");
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:ElementItem.tt:CellLayout.tt:Transformation.tt:Scale"].addAttrib("y", "0");
    }

    AWAlarmCover alarmCover;
    ret = adaptor->event_ctrl_->GetAlarmCoverConfig(h, alarmCover);
    if (ret != 0) {
        alarmCover.cover_enable = 0;
    }

//    if (1 == alarmCover.cover_enable) {
    if (true) {
        XmlBuilder *module1 = new XmlBuilder("tan:AnalyticsModule");
        resp.append(module1);
        module1->addAttrib("Name", "AWTamperEngine");
        module1->addAttrib("Type", "tt:TamperEngine");
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:SimpleItem"].addAttrib("Name", "Sensitivity");
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:SimpleItem"].addAttrib("Value", alarmCover.sensitive);
        (*module1)["tan:AnalyticsModule.tt:Parameters.tt:ElementItem"].addAttrib("Name", "Field");

        XmlBuilder &polygen = (*module1)["tan:AnalyticsModule.tt:Parameters.tt:ElementItem.tt:PolygonConfiguration.tt:Polygon"];
        XmlBuilder *point1 = new XmlBuilder("tt:Point");
        XmlBuilder *point2 = new XmlBuilder("tt:Point");
        XmlBuilder *point3 = new XmlBuilder("tt:Point");
        XmlBuilder *point4 = new XmlBuilder("tt:Point");
        polygen.append(point1);
        polygen.append(point2);
        polygen.append(point3);
        polygen.append(point4);

        point1->addAttrib("x", "0");
        point1->addAttrib("y", "240");

        point2->addAttrib("x", "320");
        point2->addAttrib("y", "240");

        point3->addAttrib("x", "320");
        point3->addAttrib("y", "0");

        point4->addAttrib("x", "0");
        point4->addAttrib("y", "0");
    }

    return 0;
}

int onvif::ModifyAnalyticsModules(const struct OnvifContext* context, XmlBuilder& req, XmlBuilder& resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);

    req["Envelope.Body.tan:ModifyAnalyticsModules.tan:ConfigurationToken"].stringValue();
    XmlBuilder &modules = req["Envelope.Body.tan:ModifyAnalyticsModules"];
    int count = modules.childCount();

    for (int i = 0; i < count; i++) {
       XmlBuilder *m = modules.getChild(i);
       if (!m->compareTag("tan:AnalyticsModule")) {
           continue;
       }

       const string &mName = (*m)["tan:AnalyticsModule"].attrib("Name");
       const string &mType = (*m)["tan:AnalyticsModule"].attrib("Type");
       const string &itemName = (*m)["tan:AnalyticsModule.tt:Parameters.tt:SimpleItem"].attrib("Name");
       const string &itemValue = (*m)["tan:AnalyticsModule.tt:Parameters.tt:SimpleItem"].attrib("Value");

#define TYPE_CELL_MODTION_ENGINE "tt:CellMotionEngine"
#define TYPE_TAMPER_ENGINE "tt:TamperEngine"

       if (itemName != "Sensitivity") {
           LOG("the item name is not sensitivity, item name: %s, item value: %s", itemName.c_str(), itemValue.c_str());
           continue;
       }

       const string fakeToken("main");
       findUseHandle(fakeToken, adaptor, h);

       if (mType == TYPE_CELL_MODTION_ENGINE) {
           AWAlarmMd md;
           adaptor->event_ctrl_->GetAlarmMdConfig(h, md);
           md.sensitive = atoi(itemValue.c_str());
           adaptor->event_ctrl_->SetAlarmMdConfig(h, md);
       } else if (mType == TYPE_TAMPER_ENGINE) {
           AWAlarmCover cv;
           adaptor->event_ctrl_->GetAlarmCoverConfig(h, cv);
           cv.sensitive = atoi(itemValue.c_str());
           adaptor->event_ctrl_->SetAlarmCoverConfig(h, cv);
       } else {
           LOG("module type is unknown! type: %s", mType.c_str());
           continue;
       }

       adaptor->event_ctrl_->SaveEventConfig();
    }


    return 0;
}

int onvif::Subscribe(const struct OnvifContext* context, XmlBuilder& req, XmlBuilder& resp) {

    return 0;
}

int onvif::UnSubscribe(const struct OnvifContext* context, XmlBuilder& req, XmlBuilder& resp) {

    return 0;
}

int buildAlarmMessage(XmlBuilder &obj) {
   SoapUtils::addVisitNamespace(obj, "wsnt:tt", SoapUtils::getAllXmlNameSpace());
   obj["Envelope.Body.wsnt:Notify.wsnt:NotificationMessage.wsnt:Topic"].addAttrib(
           "Dialect", "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
   obj["Envelope.Body.wsnt:Notify.wsnt:NotificationMessage.wsnt:Topic"] = "tns1:RuleEngine";;
   XmlBuilder &message = obj["Envelope.Body.wsnt:Notify.wsnt:NotificationMessage.wsnt:Topic.wsnt:Message"];
   // UtcTime="2016-12-09T07:25:38Z"
   message["wsnt:Message.tt:Message"].addAttrib("UtcTime", "");
   message["wsnt:Message.tt:Message"].addAttrib("PropertyOperation", "changed");
   XmlBuilder &source = message["wsnt:Message.tt:Message.tt:Source"];
   XmlBuilder *item1 = new XmlBuilder(source, "tt:SimpleItem");
   XmlBuilder *item2 = new XmlBuilder(source, "tt:SimpleItem");
   XmlBuilder *item3 = new XmlBuilder(source, "tt:SimpleItem");

   item1->addAttrib("Name", "VideoSourceConfigurationToken");
   item1->addAttrib("Value", "");

   item2->addAttrib("Name", "VideoAnalyticsConfigurationToken");
   item2->addAttrib("Value", "");

   item3->addAttrib("Name", "Rule");
   item3->addAttrib("Value", "");

   message["wsnt:Message.tt:Message.tt:Data.tt:SimpleItem"].addAttrib("Name", "IsMotion");
   message["wsnt:Message.tt:Message.tt:Data.tt:SimpleItem"].addAttrib("Value", "true");

   return 0;
}


























