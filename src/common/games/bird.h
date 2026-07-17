#ifndef PROJ_PAGE_BIRD_H
#define PROJ_PAGE_BIRD_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

/*********************
 *      DEFINES
 *********************/
typedef int key_code_t;
typedef int key_action_t;

#define KEY_CODE_HOME   0x73
#define KEY_ACTION_DOWN 1

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t *obj;
    void (*on_destroy)(void *p);
    bool (*on_key)(void *p, key_code_t key_code, key_action_t key_action);
} BasePage;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
BasePage * page_bird_create(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
