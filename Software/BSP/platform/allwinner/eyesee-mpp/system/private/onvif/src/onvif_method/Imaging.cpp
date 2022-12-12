#include "../gsoap-gen/soapStub.h"
#include "../include/utils.h"
#include "dev_ctrl_adapter.h"

int __timg__GetImagingSettings(struct soap* _soap, struct _timg__GetImagingSettings *timg__GetImagingSettings,
    struct _timg__GetImagingSettingsResponse *timg__GetImagingSettingsResponse) {
#ifdef DEBUG
  printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
  printf("get image setting source token: %s\n", timg__GetImagingSettings->VideoSourceToken);
#endif
  do {
    DeviceAdapter::ImageControl *imgCtrl = getSystemInterface(_soap)->image_ctrl_;

    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings, struct tt__ImagingSettings20);
    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings->BacklightCompensation, struct tt__BacklightCompensation20)
    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings->BacklightCompensation->Level, float)
    *timg__GetImagingSettingsResponse->ImagingSettings->BacklightCompensation->Level = 1.0;
    timg__GetImagingSettingsResponse->ImagingSettings->BacklightCompensation->Mode = tt__BacklightCompensationMode__ON;

    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings->Brightness, float)
    AWImageColour color;
    int chn = tokenToChannel(timg__GetImagingSettings->VideoSourceToken);
    int ret = imgCtrl->GetImageColour(chn, color);
    if (ret != 0)
      break;
    *timg__GetImagingSettingsResponse->ImagingSettings->Brightness = color.brightness;

    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings->ColorSaturation, float)
    *timg__GetImagingSettingsResponse->ImagingSettings->ColorSaturation = color.saturation;

    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings->Contrast, float)
    *timg__GetImagingSettingsResponse->ImagingSettings->Contrast = color.contrast;

    timg__GetImagingSettingsResponse->ImagingSettings->Exposure = NULL;
    timg__GetImagingSettingsResponse->ImagingSettings->Extension = NULL;
    timg__GetImagingSettingsResponse->ImagingSettings->Focus = NULL;
    timg__GetImagingSettingsResponse->ImagingSettings->IrCutFilter = NULL;
    timg__GetImagingSettingsResponse->ImagingSettings->Sharpness = NULL;

    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings->WhiteBalance, struct tt__WhiteBalance20)
    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings->WhiteBalance->CbGain, float)
    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings->WhiteBalance->CrGain, float)
    *timg__GetImagingSettingsResponse->ImagingSettings->WhiteBalance->CbGain = 80.0;
    *timg__GetImagingSettingsResponse->ImagingSettings->WhiteBalance->CrGain = 20.0;
    timg__GetImagingSettingsResponse->ImagingSettings->WhiteBalance->Mode = tt__WhiteBalanceMode__MANUAL;
    timg__GetImagingSettingsResponse->ImagingSettings->WhiteBalance->Extension = NULL;

    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings->WideDynamicRange, struct tt__WideDynamicRange20)
    MALLOC(timg__GetImagingSettingsResponse->ImagingSettings->WideDynamicRange->Level, float)
    *timg__GetImagingSettingsResponse->ImagingSettings->WideDynamicRange->Level = 50;
    timg__GetImagingSettingsResponse->ImagingSettings->WideDynamicRange->Mode = tt__WideDynamicMode__ON;

    return SOAP_OK;
  } while (0);
  return SOAP_ERR;
}

int __timg__SetImagingSettings(struct soap* _soap, struct _timg__SetImagingSettings *timg__SetImagingSettings,
    struct _timg__SetImagingSettingsResponse *timg__SetImagingSettingsResponse) {
#ifdef DEBUG
  printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
  printf("set image:\n"
      "token: %s\n"
      "brightness: %f\n"
      "contrast: %f\n"
      "colorsaturation: %f\n", timg__SetImagingSettings->VideoSourceToken, *timg__SetImagingSettings->ImagingSettings->Brightness,
      *timg__SetImagingSettings->ImagingSettings->Contrast, *timg__SetImagingSettings->ImagingSettings->ColorSaturation);
#endif

  DeviceAdapter::ImageControl *imgCtrl = getSystemInterface(_soap)->image_ctrl_;
  AWImageColour imageColor;
  int chn = tokenToChannel(timg__SetImagingSettings->VideoSourceToken);
  do {
    int ret = 0;
    ret = imgCtrl->GetImageColour(chn, imageColor);
    if (ret != 0)
      break;
    imageColor.brightness = (int) *timg__SetImagingSettings->ImagingSettings->Brightness;
    imageColor.saturation = (int) *timg__SetImagingSettings->ImagingSettings->ColorSaturation;
    imageColor.contrast = (int) *timg__SetImagingSettings->ImagingSettings->Contrast;

    ret = imgCtrl->SetImageColour(chn, imageColor);
    if (ret != 0)
      break;

    imgCtrl->SaveImageConfig();

    return SOAP_OK;
  } while (0);

  return SOAP_ERR;
}

int __timg__GetOptions(struct soap* _soap, struct _timg__GetOptions *timg__GetOptions, struct _timg__GetOptionsResponse *timg__GetOptionsResponse) {
#ifdef DEBUG
  printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
  printf("get options video source token: %s\n", timg__GetOptions->VideoSourceToken);
#endif

  MALLOC(timg__GetOptionsResponse->ImagingOptions, struct tt__ImagingOptions20);
  MALLOC(timg__GetOptionsResponse->ImagingOptions->BacklightCompensation, struct tt__BacklightCompensationOptions20)
  MALLOC(timg__GetOptionsResponse->ImagingOptions->BacklightCompensation->Level, struct tt__FloatRange)
  MALLOCN(timg__GetOptionsResponse->ImagingOptions->BacklightCompensation->Mode, enum tt__BacklightCompensationMode, 1)
  timg__GetOptionsResponse->ImagingOptions->BacklightCompensation->Level->Max = 100.0;
  timg__GetOptionsResponse->ImagingOptions->BacklightCompensation->Level->Min = 0.0;
  timg__GetOptionsResponse->ImagingOptions->BacklightCompensation->__sizeMode = 1;
  timg__GetOptionsResponse->ImagingOptions->BacklightCompensation->Mode[0] = tt__BacklightCompensationMode__ON;

  MALLOC(timg__GetOptionsResponse->ImagingOptions->Brightness, tt__FloatRange)
  timg__GetOptionsResponse->ImagingOptions->Brightness->Max = 100.0;
  timg__GetOptionsResponse->ImagingOptions->Brightness->Min = 0.0;

  MALLOC(timg__GetOptionsResponse->ImagingOptions->ColorSaturation, tt__FloatRange)
  timg__GetOptionsResponse->ImagingOptions->ColorSaturation->Max = 100.0;
  timg__GetOptionsResponse->ImagingOptions->ColorSaturation->Min = 0.0;

  MALLOC(timg__GetOptionsResponse->ImagingOptions->Contrast, tt__FloatRange)
  timg__GetOptionsResponse->ImagingOptions->Contrast->Max = 100.0;
  timg__GetOptionsResponse->ImagingOptions->Contrast->Min = 0.0;

  timg__GetOptionsResponse->ImagingOptions->Exposure = NULL;
  timg__GetOptionsResponse->ImagingOptions->Extension = NULL;
  timg__GetOptionsResponse->ImagingOptions->Focus = NULL;
  timg__GetOptionsResponse->ImagingOptions->__sizeIrCutFilterModes = 0;
  timg__GetOptionsResponse->ImagingOptions->IrCutFilterModes = NULL;
  timg__GetOptionsResponse->ImagingOptions->Sharpness = NULL;

  MALLOC(timg__GetOptionsResponse->ImagingOptions->WhiteBalance, struct tt__WhiteBalanceOptions20)
  MALLOC(timg__GetOptionsResponse->ImagingOptions->WhiteBalance->YbGain, tt__FloatRange)
  MALLOC(timg__GetOptionsResponse->ImagingOptions->WhiteBalance->YrGain, tt__FloatRange)
  MALLOCN(timg__GetOptionsResponse->ImagingOptions->WhiteBalance->Mode, enum tt__WhiteBalanceMode, 2)
  timg__GetOptionsResponse->ImagingOptions->WhiteBalance->YbGain->Max = 100.0;
  timg__GetOptionsResponse->ImagingOptions->WhiteBalance->YbGain->Min = 0.0;
  timg__GetOptionsResponse->ImagingOptions->WhiteBalance->YrGain->Max = 100.0;
  timg__GetOptionsResponse->ImagingOptions->WhiteBalance->YrGain->Min = 0.0;
  timg__GetOptionsResponse->ImagingOptions->WhiteBalance->__sizeMode = 1;
  timg__GetOptionsResponse->ImagingOptions->WhiteBalance->Mode[0] = tt__WhiteBalanceMode__MANUAL;
  timg__GetOptionsResponse->ImagingOptions->WhiteBalance->Mode[1] = tt__WhiteBalanceMode__AUTO;
  timg__GetOptionsResponse->ImagingOptions->WhiteBalance->Extension = NULL;

  MALLOC(timg__GetOptionsResponse->ImagingOptions->WideDynamicRange, struct tt__WideDynamicRangeOptions20)
  MALLOC(timg__GetOptionsResponse->ImagingOptions->WideDynamicRange->Level, tt__FloatRange)
  MALLOCN(timg__GetOptionsResponse->ImagingOptions->WideDynamicRange->Mode, enum tt__WideDynamicMode, 2)
  timg__GetOptionsResponse->ImagingOptions->WideDynamicRange->Level->Max = 100.0;
  timg__GetOptionsResponse->ImagingOptions->WideDynamicRange->Level->Min = 0.0;
  timg__GetOptionsResponse->ImagingOptions->WideDynamicRange->__sizeMode = 1;
  timg__GetOptionsResponse->ImagingOptions->WideDynamicRange->Mode[0] = tt__WideDynamicMode__ON;
  timg__GetOptionsResponse->ImagingOptions->WideDynamicRange->Mode[1] = tt__WideDynamicMode__OFF;

  return SOAP_OK;
}

int __timg__GetPresets(struct soap* _soap, struct _timg__GetPresets *timg__GetPresets, struct _timg__GetPresetsResponse *timg__GetPresetsResponse) {
#ifdef DEBUG
  printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
#endif
  return SOAP_OK;
}

int __timg__GetCurrentPreset(struct soap* _soap, struct _timg__GetCurrentPreset *timg__GetCurrentPreset,
    struct _timg__GetCurrentPresetResponse *timg__GetCurrentPresetResponse) {
#ifdef DEBUG
  printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
#endif
  return SOAP_OK;
}

int __timg__SetCurrentPreset(struct soap* _soap, struct _timg__SetCurrentPreset *timg__SetCurrentPreset,
    struct _timg__SetCurrentPresetResponse *timg__SetCurrentPresetResponse) {
#ifdef DEBUG
  printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
#endif
  return SOAP_OK;
}

