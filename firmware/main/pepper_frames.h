#pragma once

#include "lvgl.h"

#if defined(LVGL_VERSION_MAJOR) && (LVGL_VERSION_MAJOR >= 9)
typedef lv_image_dsc_t pepper_img_dsc_t;
#else
typedef lv_img_dsc_t pepper_img_dsc_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern const pepper_img_dsc_t sleepy_0;
extern const pepper_img_dsc_t sleepy_1;
extern const pepper_img_dsc_t sleepy_2;
extern const pepper_img_dsc_t work_0;
extern const pepper_img_dsc_t work_1;
extern const pepper_img_dsc_t work_2;
extern const pepper_img_dsc_t dizzy_0;
extern const pepper_img_dsc_t dizzy_1;
extern const pepper_img_dsc_t dizzy_2;

#define img_sleepy_0 sleepy_0
#define img_sleepy_1 sleepy_1
#define img_sleepy_2 sleepy_2
#define img_work_0   work_0
#define img_work_1   work_1
#define img_work_2   work_2
#define img_dizzy_0  dizzy_0
#define img_dizzy_1  dizzy_1
#define img_dizzy_2  dizzy_2

#ifdef __cplusplus
}
#endif
