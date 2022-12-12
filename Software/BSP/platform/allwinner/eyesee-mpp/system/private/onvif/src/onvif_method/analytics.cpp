/******************************************************************************
 Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************/

#include "../gsoap-gen/soapStub.h"
#include "dev_ctrl_adapter.h"
#include "Packbits.h"
#include <sstream>

#include "../include/utils.h"

using namespace std;

string int2str(int val) {
    ostringstream out;
    out << val;
    return out.str();
}

int str2int(const string &val) {
    istringstream in(val.c_str());
    int ret = 0;
    in >> ret;
    return ret;
}

enum SimpleItemType {
    SimpleItemTypeSensitivity, SimpleItemTypeActiveCells, SimpleItemTypeUnknow
};

SimpleItemType parseSimpleItemType(struct _tt__ItemList_SimpleItem item) {
    if (0 == strcmp(item.Name, "Sensitivity")) {
        return SimpleItemTypeSensitivity;
    } else if (0 == strcmp(item.Name, "ActiveCells")) {
        return SimpleItemTypeActiveCells;
    } else if (0 == strcmp(item.Name, "MinCount")) {
        return SimpleItemTypeUnknow;
    } else if (0 == strcmp(item.Name, "AlarmOnDelay")) {
        return SimpleItemTypeUnknow;
    } else {
        return SimpleItemTypeUnknow;
    }
}

enum AnalyticsModuleType {
    AnalyticsModuleMotionDectect, AnalyticsModuleTamperDectect, AnalyticsModuleUnknow
};

AnalyticsModuleType parseAnalyticsModuleType(char *type) {
    if (0 == strcmp(type, "tt:CellMotionEngine")) {
        return AnalyticsModuleMotionDectect;
    } else if (0 == strcmp(type, "tt:TamperEngine")) {
        return AnalyticsModuleTamperDectect;
    } else {
        return AnalyticsModuleUnknow;
    }
}

int __tad__DeleteAnalyticsEngineControl(struct soap* _soap,
        struct _tad__DeleteAnalyticsEngineControl *tad__DeleteAnalyticsEngineControl,
        struct _tad__DeleteAnalyticsEngineControlResponse *tad__DeleteAnalyticsEngineControlResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__CreateAnalyticsEngineControl(struct soap* _soap,
        struct _tad__CreateAnalyticsEngineControl *tad__CreateAnalyticsEngineControl,
        struct _tad__CreateAnalyticsEngineControlResponse *tad__CreateAnalyticsEngineControlResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__SetAnalyticsEngineControl(struct soap* _soap,
        struct _tad__SetAnalyticsEngineControl *tad__SetAnalyticsEngineControl,
        struct _tad__SetAnalyticsEngineControlResponse *tad__SetAnalyticsEngineControlResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__GetAnalyticsEngineControl(struct soap* _soap,
        struct _tad__GetAnalyticsEngineControl *tad__GetAnalyticsEngineControl,
        struct _tad__GetAnalyticsEngineControlResponse *tad__GetAnalyticsEngineControlResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__GetAnalyticsEngineControls(struct soap* _soap,
        struct _tad__GetAnalyticsEngineControls *tad__GetAnalyticsEngineControls,
        struct _tad__GetAnalyticsEngineControlsResponse *tad__GetAnalyticsEngineControlsResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__GetAnalyticsEngine(struct soap* _soap, struct _tad__GetAnalyticsEngine *tad__GetAnalyticsEngine,
        struct _tad__GetAnalyticsEngineResponse *tad__GetAnalyticsEngineResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__GetAnalyticsEngines(struct soap* _soap, struct _tad__GetAnalyticsEngines *tad__GetAnalyticsEngines,
        struct _tad__GetAnalyticsEnginesResponse *tad__GetAnalyticsEnginesResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__SetVideoAnalyticsConfiguration(struct soap* _soap,
        struct _tad__SetVideoAnalyticsConfiguration *tad__SetVideoAnalyticsConfiguration,
        struct _tad__SetVideoAnalyticsConfigurationResponse *tad__SetVideoAnalyticsConfigurationResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__SetAnalyticsEngineInput(struct soap* _soap,
        struct _tad__SetAnalyticsEngineInput *tad__SetAnalyticsEngineInput,
        struct _tad__SetAnalyticsEngineInputResponse *tad__SetAnalyticsEngineInputResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__GetAnalyticsEngineInput(struct soap* _soap,
        struct _tad__GetAnalyticsEngineInput *tad__GetAnalyticsEngineInput,
        struct _tad__GetAnalyticsEngineInputResponse *tad__GetAnalyticsEngineInputResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__GetAnalyticsEngineInputs(struct soap* _soap,
        struct _tad__GetAnalyticsEngineInputs *tad__GetAnalyticsEngineInputs,
        struct _tad__GetAnalyticsEngineInputsResponse *tad__GetAnalyticsEngineInputsResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__GetAnalyticsDeviceStreamUri(struct soap* _soap,
        struct _tad__GetAnalyticsDeviceStreamUri *tad__GetAnalyticsDeviceStreamUri,
        struct _tad__GetAnalyticsDeviceStreamUriResponse *tad__GetAnalyticsDeviceStreamUriResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__GetVideoAnalyticsConfiguration(struct soap* _soap, struct _tad__GetVideoAnalyticsConfiguration *req,
        struct _tad__GetVideoAnalyticsConfigurationResponse *resp) {
    printf("function: %s invoked!\n", __FUNCTION__);
    printf("get video analytics configuration use token: %s\n", req->ConfigurationToken);
    MALLOC(resp->Configuration, struct tt__VideoAnalyticsConfiguration);
    MALLOC(resp->Configuration->RuleEngineConfiguration, struct tt__RuleEngineConfiguration);
    MALLOC(resp->Configuration->AnalyticsEngineConfiguration, struct tt__AnalyticsEngineConfiguration);

    resp->Configuration->Name = "GetVideoAnalyticsConfiguration";
    resp->Configuration->token = req->ConfigurationToken;
    resp->Configuration->UseCount = 1;

    struct _tan__GetRules getRules;
    struct _tan__GetRulesResponse getRulesResponse;
    getRules.ConfigurationToken = req->ConfigurationToken;
    __tan__GetRules(_soap, &getRules, &getRulesResponse);
    resp->Configuration->RuleEngineConfiguration->__sizeRule = getRulesResponse.__sizeRule;
    resp->Configuration->RuleEngineConfiguration->Rule = getRulesResponse.Rule;

    struct _tan__GetAnalyticsModules getAnalyticsModules;
    struct _tan__GetAnalyticsModulesResponse getAnalyticsModulesResponse;
    getAnalyticsModules.ConfigurationToken = req->ConfigurationToken;
    __tan__GetAnalyticsModules(_soap, &getAnalyticsModules, &getAnalyticsModulesResponse);
    resp->Configuration->AnalyticsEngineConfiguration->__sizeAnalyticsModule =
            getAnalyticsModulesResponse.__sizeAnalyticsModule;
    resp->Configuration->AnalyticsEngineConfiguration->AnalyticsModule = getAnalyticsModulesResponse.AnalyticsModule;

    return SOAP_OK;
}

int __tad__CreateAnalyticsEngineInputs(struct soap* _soap,
        struct _tad__CreateAnalyticsEngineInputs *tad__CreateAnalyticsEngineInputs,
        struct _tad__CreateAnalyticsEngineInputsResponse *tad__CreateAnalyticsEngineInputsResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__DeleteAnalyticsEngineInputs(struct soap* _soap,
        struct _tad__DeleteAnalyticsEngineInputs *tad__DeleteAnalyticsEngineInputs,
        struct _tad__DeleteAnalyticsEngineInputsResponse *tad__DeleteAnalyticsEngineInputsResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tad__GetAnalyticsState(struct soap* _soap, struct _tad__GetAnalyticsState *tad__GetAnalyticsState,
        struct _tad__GetAnalyticsStateResponse *tad__GetAnalyticsStateResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tan__GetSupportedRules(struct soap* _soap, struct _tan__GetSupportedRules *tan__GetSupportedRules,
        struct _tan__GetSupportedRulesResponse *tan__GetSupportedRulesResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

void filledDetectArray(struct soap *_soap, AWAlarmMd &alarmMd, const char *base64) {
    int packDataSize = 0;
    const char *packData = soap_base642s(_soap, base64, NULL, 0, &packDataSize);
    unsigned char detectArray[sizeof(alarmMd.detec_area)];
    memset(alarmMd.detec_area, 0, sizeof(alarmMd.detec_area));
    tiff6_unPackBits(packData, packDataSize, (unsigned char*) alarmMd.detec_area);
}

// TODO:目前无法判断NVR的设置，例如：四组区域设置，无法确定使用哪一个
int __tan__CreateRules(struct soap* _soap, struct _tan__CreateRules *req, struct _tan__CreateRulesResponse *resp) {
    printf("function: %s invoked!\n", __FUNCTION__);
    printf("configuration token: %s\n", req->ConfigurationToken);
    printf("simple item size: %d\n", req->Rule->Parameters->__sizeSimpleItem);
    printf("element item size: %d\n", req->Rule->Parameters->__sizeElementItem);
    printf("rule name: %s\n" "rule type: %s\n", req->Rule->Name, req->Rule->Type);

    for (int i = 0; i < req->Rule->Parameters->__sizeSimpleItem; i++) {
        printf("\t{ name: %s value: %s}\n", req->Rule->Parameters->SimpleItem[i].Name,
                req->Rule->Parameters->SimpleItem[i].Value);
        SimpleItemType type = parseSimpleItemType(req->Rule->Parameters->SimpleItem[i]);
        switch (type) {
            case SimpleItemTypeActiveCells: {
                if (NULL != strstr(req->Rule->Name, "AWCellMotionDetector")) {
                    printf("invoke SaveEventConfig!\n");
                    AWAlarmMd alarmMd;
                    int ret = getSystemInterface(_soap)->event_ctrl_->GetAlarmMdConfig(0, alarmMd);
                    if (ret != 0) {
                        printf("GetAlarmMdConfig failed!\n");
                        break;
                    }
                    filledDetectArray(_soap, alarmMd, req->Rule->Parameters->SimpleItem[i].Value);
                    ret = getSystemInterface(_soap)->event_ctrl_->SetAlarmMdConfig(0, alarmMd);
                    if (ret != 0) {
                        printf("SetAlarmMdConfig failed!\n");
                        break;
                    }
                    getSystemInterface(_soap)->event_ctrl_->SaveEventConfig();
                }
                break;
            }
            case SimpleItemTypeSensitivity:
                break;
            default:
                break;
        }
    }

    return SOAP_OK;
}

int __tan__DeleteRules(struct soap* _soap, struct _tan__DeleteRules *req, struct _tan__DeleteRulesResponse *resp) {
    printf("function: %s invoked!\n configuration token: %s\n", __FUNCTION__, req->ConfigurationToken);
    for (int i = 0; i < req->__sizeRuleName; i++) {
        printf("delete rules, rules name: %s \n", req->RuleName[i]);
    }

    return SOAP_OK;
}

char *getActiveCells(struct soap* _soap, AWAlarmMd &alarmMd) {
    char *packData = NULL;
    MALLOCN(packData, char, INFO_LENGTH)
    int packDataSize = tiff6_PackBits((unsigned char*) alarmMd.detec_area, sizeof(alarmMd.detec_area),
            (unsigned char*) packData);
    int base64DataSize = 0;
    char *base64Data = soap_s2base64(_soap, (unsigned char*) packData, NULL, packDataSize);
    printf("packBit base64 output %s\n", base64Data);
    return base64Data;
}

int __tan__GetRules(struct soap* _soap, struct _tan__GetRules *req, struct _tan__GetRulesResponse *resp) {
    printf("function: %s invoked!\n", __FUNCTION__);
    printf("rules token: %s\n", req->ConfigurationToken);

    MALLOCN(resp->Rule, struct tt__Config, 2)
    MALLOC(resp->Rule[0].Parameters, struct tt__ItemList)
    MALLOCN(resp->Rule[0].Parameters->SimpleItem, struct _tt__ItemList_SimpleItem, 4)

    do {
        AWAlarmMd alarmMd;
        int ret = getSystemInterface(_soap)->event_ctrl_->GetAlarmMdConfig(0, alarmMd);
        if (ret != 0)
            break;

        resp->__sizeRule = 2;
        resp->Rule[0].Name = "AWCellMotionDetector";
        resp->Rule[0].Type = "tt:CellMotionDetector";
        resp->Rule[0].Parameters->__sizeSimpleItem = 4;
        resp->Rule[0].Parameters->SimpleItem[0].Name = "MinCount";
        resp->Rule[0].Parameters->SimpleItem[0].Value = "5";
        resp->Rule[0].Parameters->SimpleItem[1].Name = "AlarmOnDelay";
        resp->Rule[0].Parameters->SimpleItem[1].Value = "1000";
        resp->Rule[0].Parameters->SimpleItem[2].Name = "AlarmOffDelay";
        resp->Rule[0].Parameters->SimpleItem[2].Value = "1000";
        resp->Rule[0].Parameters->SimpleItem[3].Name = "ActiveCells";
        resp->Rule[0].Parameters->SimpleItem[3].Value = getActiveCells(_soap, alarmMd);
        resp->Rule[0].Parameters->__sizeElementItem = 0;
        resp->Rule[0].Parameters->ElementItem = NULL;
        resp->Rule[0].Parameters->Extension = NULL;

        MALLOC(resp->Rule[1].Parameters, struct tt__ItemList)
        MALLOCN(resp->Rule[1].Parameters->ElementItem, struct _tt__ItemList_ElementItem, 1)
        MALLOCN(resp->Rule[1].Parameters->ElementItem->PolygonConfiguration, struct tt__PolygonConfiguration, 1)
        MALLOC(resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon, struct tt__Polygon)
        MALLOCN(resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point, struct tt__Vector, 4)
        for (int i = 0; i < 4; i++) {
            MALLOC(resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point[i].x, float);
            MALLOC(resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point[i].y, float);
        }

        resp->Rule[1].Name = "AWTamperDetector";
        resp->Rule[1].Type = "tt:TamperDetector";
        resp->Rule[1].Parameters->__sizeSimpleItem = 0;
        resp->Rule[1].Parameters->SimpleItem = NULL;
        resp->Rule[1].Parameters->__sizeElementItem = 1;
        resp->Rule[1].Parameters->ElementItem->Name = "Field";
        resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->__sizePoint = 4;
        *resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point[0].x = 0;
        *resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point[0].y = 0;
        *resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point[1].x = 320;
        *resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point[1].y = 0;
        *resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point[2].x = 320;
        *resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point[2].y = 240;
        *resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point[3].x = 0;
        *resp->Rule[1].Parameters->ElementItem->PolygonConfiguration->Polygon->Point[3].y = 240;
        resp->Rule[1].Parameters->ElementItem->CellLayout =
        NULL;
        resp->Rule[1].Parameters->ElementItem->Transformation =
        NULL;
        resp->Rule[1].Parameters->Extension = NULL;

        return SOAP_OK;
    } while (0);

    return SOAP_ERR;
}

int __tan__ModifyRules(struct soap* _soap, struct _tan__ModifyRules *req, struct _tan__ModifyRulesResponse *resp) {
    printf("function: %s invoked!\n", __FUNCTION__);
    printf("configuration token: %s\n", req->ConfigurationToken);
    printf("simple item size: %d\n", req->Rule->Parameters->__sizeSimpleItem);
    printf("rule name: %s\n" "rule type: %s\n", req->Rule->Name, req->Rule->Type);

    do {
        AWAlarmMd alarmMd;
        int ret = getSystemInterface(_soap)->event_ctrl_->GetAlarmMdConfig(0, alarmMd);
        if (ret != 0)
            break;

        for (int i = 0; i < req->Rule->Parameters->__sizeSimpleItem; i++) {
            printf("\t{ name: %s value: %s}\n", req->Rule->Parameters->SimpleItem[i].Name,
                    req->Rule->Parameters->SimpleItem[i].Value);
            SimpleItemType type = parseSimpleItemType(req->Rule->Parameters->SimpleItem[i]);
            switch (type) {
                case SimpleItemTypeActiveCells:
                    break;
                case SimpleItemTypeSensitivity:
                    break;
                default:
                    break;
            }
        }

        printf("element item size: %d\n", req->Rule->Parameters->__sizeElementItem);
        for (int i = 0; i < req->Rule->Parameters->__sizeElementItem; i++) {
            printf("\t{ name: %s}\n", req->Rule->Parameters->ElementItem[i].Name);
        }

        getSystemInterface(_soap)->event_ctrl_->SaveEventConfig();

        return SOAP_OK;
    } while (0);

    return SOAP_ERR;
}

int __tan__GetSupportedAnalyticsModules(struct soap* _soap,
        struct _tan__GetSupportedAnalyticsModules *tan__GetSupportedAnalyticsModules,
        struct _tan__GetSupportedAnalyticsModulesResponse *tan__GetSupportedAnalyticsModulesResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tan__CreateAnalyticsModules(struct soap* _soap, struct _tan__CreateAnalyticsModules *tan__CreateAnalyticsModules,
        struct _tan__CreateAnalyticsModulesResponse *tan__CreateAnalyticsModulesResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tan__DeleteAnalyticsModules(struct soap* _soap, struct _tan__DeleteAnalyticsModules *tan__DeleteAnalyticsModules,
        struct _tan__DeleteAnalyticsModulesResponse *tan__DeleteAnalyticsModulesResponse) {
    printf("function: %s invoked!\n", __FUNCTION__);
    return SOAP_OK;
}

int __tan__GetAnalyticsModules(struct soap* _soap, struct _tan__GetAnalyticsModules *req,
        struct _tan__GetAnalyticsModulesResponse *resp) {
    printf("function: %s invoked!\n", __FUNCTION__);
    printf("analyticsModules token: %s\n", req->ConfigurationToken);

    do {
        MALLOCN(resp->AnalyticsModule, struct tt__Config, 2)
        MALLOC(resp->AnalyticsModule[0].Parameters, struct tt__ItemList)
        MALLOC(resp->AnalyticsModule[0].Parameters->SimpleItem, struct _tt__ItemList_SimpleItem)
        MALLOCN(resp->AnalyticsModule[0].Parameters->ElementItem, struct _tt__ItemList_ElementItem, 1)
        MALLOC(resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout, struct tt__CellLayout)
        MALLOC(resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation,
                struct tt__Transformation)
        MALLOC(resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Scale, struct tt__Vector)
        MALLOC(resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Scale->x, float)
        MALLOC(resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Scale->y, float)
        MALLOC(resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Translate,
                struct tt__Vector)
        MALLOC(resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Translate->x, float)
        MALLOC(resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Translate->y, float)

        AWAlarmMd alarmMd;
        int ret = getSystemInterface(_soap)->event_ctrl_->GetAlarmMdConfig(0, alarmMd);
        if (ret != 0)
            break;
        resp->__sizeAnalyticsModule = 2;
        resp->AnalyticsModule[0].Name = "AWCellMotion";
        resp->AnalyticsModule[0].Type = "tt:CellMotionEngine";
        resp->AnalyticsModule[0].Parameters->__sizeSimpleItem = 1;
        resp->AnalyticsModule[0].Parameters->SimpleItem[0].Name = "Sensitivity";
        MALLOCN(resp->AnalyticsModule[0].Parameters->SimpleItem[0].Value, char, INFO_LENGTH);
        string mdSensitiveStr = int2str(alarmMd.sensitive);
        strncpy(resp->AnalyticsModule[0].Parameters->SimpleItem[0].Value, mdSensitiveStr.c_str(),
                mdSensitiveStr.size());
        resp->AnalyticsModule[0].Parameters->__sizeElementItem = 1;
        resp->AnalyticsModule[0].Parameters->ElementItem[0].Name = "Layout";
        resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Columns = "22";
        resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Rows = "18";
        *resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Scale->x = 0.0;
        *resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Scale->y = 0.0;
        *resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Translate->x = 1.0;
        *resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Translate->x = 1.0;
        resp->AnalyticsModule[0].Parameters->ElementItem[0].CellLayout->Transformation->Extension =
        NULL;
        resp->AnalyticsModule[0].Parameters->ElementItem[0].PolygonConfiguration =
        NULL;
        resp->AnalyticsModule[0].Parameters->ElementItem[0].Transformation =
        NULL;
        resp->AnalyticsModule[0].Parameters->Extension =
        NULL;

        MALLOC(resp->AnalyticsModule[1].Parameters, struct tt__ItemList)
        MALLOC(resp->AnalyticsModule[1].Parameters->SimpleItem, struct _tt__ItemList_SimpleItem)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem, struct _tt__ItemList_ElementItem)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration,
                struct tt__PolygonConfiguration)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon, struct tt__Polygon)
        MALLOCN(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point,
                struct tt__Vector, 4)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[0].x, float)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[0].y, float)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[1].x, float)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[1].y, float)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[2].x, float)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[2].y, float)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[3].x, float)
        MALLOC(resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[3].y, float)

        AWAlarmCover alarmCover;
        ret = getSystemInterface(_soap)->event_ctrl_->GetAlarmCoverConfig(0, alarmCover);
        if (ret != 0)
            break;
        resp->AnalyticsModule[1].Name = "AWTamperEngine";
        resp->AnalyticsModule[1].Type = "tt:TamperEngine";
        resp->AnalyticsModule[1].Parameters->__sizeSimpleItem = 1;
        resp->AnalyticsModule[1].Parameters->SimpleItem[0].Name = "Sensitivity";
        MALLOCN(resp->AnalyticsModule[1].Parameters->SimpleItem[0].Value, char, INFO_LENGTH);
        string teSensitiveStr = int2str(alarmCover.sensitive);
        strncpy(resp->AnalyticsModule[1].Parameters->SimpleItem[0].Value, teSensitiveStr.c_str(),
                teSensitiveStr.size());
        resp->AnalyticsModule[1].Parameters->__sizeElementItem = 1;
        resp->AnalyticsModule[1].Parameters->ElementItem[0].Name = "Field";
        resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->__sizePoint = 4;
        *resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[0].x = 0.0;
        *resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[0].y = 240.0;
        *resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[1].x = 320.0;
        *resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[1].y = 240.0;
        *resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[2].x = 320.0;
        *resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[2].y = 0.0;
        *resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[3].x = 0.0;
        *resp->AnalyticsModule[1].Parameters->ElementItem[0].PolygonConfiguration->Polygon->Point[3].y = 0.0;
        resp->AnalyticsModule[1].Parameters->ElementItem[0].CellLayout =
        NULL;
        ;
        resp->AnalyticsModule[1].Parameters->ElementItem[0].Transformation =
        NULL;
        ;
        resp->AnalyticsModule[1].Parameters->Extension =
        NULL;

        return SOAP_OK;
    } while (0);

    return SOAP_ERR;
}

int __tan__ModifyAnalyticsModules(struct soap* _soap, struct _tan__ModifyAnalyticsModules *req,
        struct _tan__ModifyAnalyticsModulesResponse *resp) {
    printf("function: %s invoked!\n", __FUNCTION__);

    _tan__ModifyAnalyticsModules *module = req;
    printf("AnalyticsModule size: %d\n", module->__sizeAnalyticsModule);
    for (int i = 0; i < module->__sizeAnalyticsModule; i++) {
        printf("modify analytics module use token: %s\n", module->ConfigurationToken);
        printf("module name: %s\n module type: %s\n", module->AnalyticsModule[i].Name, module->AnalyticsModule[i].Type);
        printf("\tsimple item size: %d\n", module->AnalyticsModule->Parameters->__sizeSimpleItem);
        for (int j = 0; j < module->AnalyticsModule->Parameters->__sizeSimpleItem; j++) {
            printf("\t{ name: %s value: %s}\n", module->AnalyticsModule[i].Parameters->SimpleItem[j].Name,
                    module->AnalyticsModule[i].Parameters->SimpleItem[j].Value);

            if (strcmp(module->AnalyticsModule[i].Name, "AWCellMotion") != 0) continue;

            SimpleItemType itemType = parseSimpleItemType(module->AnalyticsModule[i].Parameters->SimpleItem[j]);
            switch (itemType) {
                case SimpleItemTypeSensitivity: {
                    char *sensitivity = NULL;
                    sensitivity = module->AnalyticsModule[i].Parameters->SimpleItem[j].Value;
                    AnalyticsModuleType moduleType = parseAnalyticsModuleType(module->AnalyticsModule[i].Type);
                    switch (moduleType) {
                        case AnalyticsModuleMotionDectect: {
                            AWAlarmMd alarmMd;
                            int ret = getSystemInterface(_soap)->event_ctrl_->GetAlarmMdConfig(0, alarmMd);
                            if (ret != 0)
                                return SOAP_ERR;
                            alarmMd.sensitive = str2int(sensitivity);
                            ret = getSystemInterface(_soap)->event_ctrl_->SetAlarmMdConfig(0, alarmMd);
                            if (ret != 0)
                                return SOAP_ERR;
                            break;
                        }
                        case AnalyticsModuleTamperDectect: {
                            AWAlarmCover alarmCover;
                            int ret = getSystemInterface(_soap)->event_ctrl_->GetAlarmCoverConfig(0, alarmCover);
                            if (ret != 0)
                                return SOAP_ERR;
                            alarmCover.sensitive = str2int(module->AnalyticsModule[i].Parameters->SimpleItem[j].Value);
                            ret = getSystemInterface(_soap)->event_ctrl_->SetAlarmCoverConfig(0, alarmCover);
                            if (ret != 0)
                                return SOAP_ERR;
                            break;
                        }
                        case AnalyticsModuleUnknow:
                            break;
                        default:
                            break;
                    };
                    getSystemInterface(_soap)->event_ctrl_->SaveEventConfig();
                    break;
                }
                case SimpleItemTypeActiveCells:
                    printf("case SimpleItemTypeActiveCells not implement!\n");
                    break;
                case SimpleItemTypeUnknow:
                    printf("case SimpleItemTypeUnknow not implement!\n");
                    break;
                default:
                    break;
            };
        }

        printf("\telement item size: %d\n", module->AnalyticsModule[i].Parameters->__sizeElementItem);
        for (int j = 0; j < module->AnalyticsModule[i].Parameters->__sizeElementItem; j++) {
            if (module->AnalyticsModule[i].Parameters->ElementItem[j].CellLayout != NULL) {
                fprintf(stderr, "\t{ name: %s\n"
                        "\tColumns: %s\n"
                        "\tRows: %s\n"
                        "\tScale x: %f\n"
                        "\tScale y: %f\n"
                        "\tTranslate x: %f\n"
                        "\tTranslate y: %f\n"
                        "\t}\n", module->AnalyticsModule[i].Parameters->ElementItem[j].Name,
                        module->AnalyticsModule[i].Parameters->ElementItem[j].CellLayout->Columns,
                        module->AnalyticsModule[i].Parameters->ElementItem[j].CellLayout->Rows,
                        *module->AnalyticsModule[i].Parameters->ElementItem[j].CellLayout->Transformation->Scale->x,
                        *module->AnalyticsModule[i].Parameters->ElementItem[j].CellLayout->Transformation->Scale->y,
                        *module->AnalyticsModule[i].Parameters->ElementItem[j].CellLayout->Transformation->Translate->x,
                        *module->AnalyticsModule[i].Parameters->ElementItem[j].CellLayout->Transformation->Translate->y);
            }
            if (module->AnalyticsModule[i].Parameters->ElementItem[j].PolygonConfiguration != NULL) {
                printf("\tname: %s\n", module->AnalyticsModule[i].Parameters->ElementItem[j].Name);

                int size =
                        module->AnalyticsModule[i].Parameters->ElementItem[j].PolygonConfiguration->Polygon->__sizePoint;
                for (int k = 0; k < size; k++) {
                    printf("\t Polygon_%d x: %f\n", k,
                            *module->AnalyticsModule[i].Parameters->ElementItem[j].PolygonConfiguration->Polygon->Point[k].x);
                    printf("\t Polygon_%d y: %f\n", k,
                            *module->AnalyticsModule[i].Parameters->ElementItem[j].PolygonConfiguration->Polygon->Point[k].y);
                }

            }
            //TODO: Transformation 海康NVR会发送该信息
        }
    }

    return SOAP_OK;
}

