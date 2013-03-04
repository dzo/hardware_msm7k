/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#define LOG_NDEBUG 0
#define LOG_TAG "lights"

#include <cutils/log.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

/******************************************************************************/

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_haveTrackballLight = 0;
static struct light_state_t g_notification;
static struct light_state_t g_battery;
static int g_backlight = 0;
static int g_trackball = -1;
static int g_buttons = 0;
static struct light_state_t g_attention;
static int g_haveAmberLed = 0;
static int g_buttonschanged=0;

char const*const TRACKBALL_FILE
        = "/sys/class/leds/jogball-backlight/brightness";

char const*const RED_LED_FILE
        = "/sys/class/leds/red/brightness";

char const*const GREEN_LED_FILE
        = "/sys/class/leds/green/brightness";

char const*const BLUE_LED_FILE
        = "/sys/class/leds/blue/brightness";

char const*const AMBER_LED_FILE
        = "/sys/class/leds/amber/brightness";

char const*const LCD_FILE
        = "/sys/class/leds/lcd-backlight/brightness";

char const*const RED_FREQ_FILE
        = "/sys/class/leds/red/device/grpfreq";

char const*const RED_PWM_FILE
        = "/sys/class/leds/red/device/grppwm";

char const*const RED_BLINK_FILE
        = "/sys/class/leds/red/blink";

char const*const GREEN_BLINK_FILE
        = "/sys/class/leds/green/blink";

char const*const BLUE_BLINK_FILE
        = "/sys/class/leds/blue/blink";

char const*const AMBER_BLINK_FILE
        = "/sys/class/leds/amber/blink";

char const*const KEYBOARD_FILE
        = "/sys/class/leds/keyboard-backlight/brightness";

char const*const BUTTON_FILE
        = "/sys/class/leds/button-backlight/brightness";

char const*const SLEEP_FILE
        = "/sys/module/RGB_led/parameters/off_when_suspended";

extern void huawei_oem_rapi_streaming_function(int,int,int,int,int *,int *,int *);

/**
 * device methods
 */

void init_globals(void)
{
    // init the mutex
    pthread_mutex_init(&g_lock, NULL);

    // figure out if we have the trackball LED or not
    g_haveTrackballLight = (access(TRACKBALL_FILE, W_OK) == 0) ? 1 : 0;

    /* figure out if we have the amber LED or not.
       If yes, just support green and amber.         */
    g_haveAmberLed = (access(AMBER_LED_FILE, W_OK) == 0) ? 1 : 0;
}

static int
write_int(char const* path, int value)
{
    int fd;
    static int already_warned = 0;

    LOGV("write_int %s %d",path,value);

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            LOGE("write_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int
read_int(char const* path)
{
    int fd;
    static int already_warned = 0;
    int value;

    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        char buffer[20];
	int amt = read(fd, buffer, 20);
        sscanf(buffer, "%d", &value);
	LOGI("read %d from %s",value,path);
        close(fd);
        return amt == -1 ? -errno : value;
    } else {
        if (already_warned == 0) {
            LOGE("read_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}



static int
is_lit(struct light_state_t const* state)
{
    return state->color & 0x00ffffff;
}


static int
rgb_to_brightness(struct light_state_t const* state)
{
    int color = state->color & 0x00ffffff;
    return ((77*((color>>16)&0x00ff))
            + (150*((color>>8)&0x00ff)) + (29*(color&0x00ff))) >> 8;
}

static int
set_light_backlight(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);
//    LOGI("set_light_backlight %d -> %d\n",g_backlight,brightness);
    pthread_mutex_lock(&g_lock);
/*    if(g_backlight==0 && brightness)
	err = write_int(BUTTON_FILE, 255);
    if(brightness==0)
        err = write_int(BUTTON_FILE, 0);
*/
    g_backlight = brightness;
    err = write_int(LCD_FILE, brightness);
//    if(g_buttonschanged==0)
    if(brightness>0 && brightness<48)
        brightness=48;

    write_int(BUTTON_FILE, brightness);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int
set_light_keyboard(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    int on = is_lit(state);
    pthread_mutex_lock(&g_lock);
    err = write_int(KEYBOARD_FILE, on?255:0);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int
set_light_buttons(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    int on = rgb_to_brightness(state);
    if(on) g_buttonschanged=1;
    if(on>0 && on<48)
        on=48;

    LOGI("set_light_buttons %d -> %d\n",g_buttons,on);

    pthread_mutex_lock(&g_lock);
    g_buttons = on;
//    err = write_int(BUTTON_FILE, on);
    pthread_mutex_unlock(&g_lock);
    return err;
}


static int
set_speaker_light_locked(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int len;
    int alpha, red, green, blue;
    int freq, pwm;
    int onMS, offMS;
    unsigned int colorRGB;
    static int blink;

    switch (state->flashMode) {
        case LIGHT_FLASH_HARDWARE:
        case LIGHT_FLASH_TIMED:
            onMS = state->flashOnMS;
            offMS = state->flashOffMS;
            break;
        case LIGHT_FLASH_NONE:
        default:
            onMS = 0;
            offMS = 0;
            break;
    }

    colorRGB = state->color;

#if 1
    LOGV("set_speaker_light_locked colorRGB=%08X, onMS=%d, offMS=%d\n",
            colorRGB, onMS, offMS);
#endif

    red=green=blue=0;

    if((colorRGB >> 16) & 0xFF) red=1;
    if((colorRGB >> 8) & 0xFF) green=1;
    if(colorRGB & 0xFF) blue=1;

/*
    red = (colorRGB >> 16) & 0xFF;
    green = (colorRGB >> 8) & 0xFF;
    blue = colorRGB & 0xFF;
*/

    write_int(RED_LED_FILE, red);
    write_int(GREEN_LED_FILE, green);
    write_int(BLUE_LED_FILE, blue);
    usleep(10*1000);

    if (state->flashMode != LIGHT_FLASH_NONE) {
        if(red)
            write_int(RED_BLINK_FILE,1);
        if(green)
            write_int(GREEN_BLINK_FILE,1);
        if(blue)
            write_int(BLUE_BLINK_FILE,1);
    }
/*
    if (onMS > 0 && offMS > 0) {
	int v[3];
        int totalMS = onMS + offMS;

        blink = 1;
        LOGI("Blink %d %d",onMS,offMS);
        v[0]=colorRGB;
        v[1]=onMS;
        v[2]=offMS;
        huawei_oem_rapi_streaming_function(0x26,0,0,0xc,v,0,0);
    } else {
	int v[3];
        blink = 0;
        freq = 0;
        pwm = 0;
        LOGI("No Blink");
        v[0]=colorRGB;
        v[1]=0;
        v[2]=0;
        huawei_oem_rapi_streaming_function(0x26,0,0,0xc,v,0,0);
    }
*/

    return 0;
}

static void
handle_speaker_battery_locked(struct light_device_t* dev)
{
    if (is_lit(&g_battery)) {
        set_speaker_light_locked(dev, &g_battery);
    } else 
	set_speaker_light_locked(dev, &g_notification);
/*if (is_lit(&g_notification)) {
        set_speaker_light_locked(dev, &g_notification);
    } else {
        set_speaker_light_locked(dev, &g_attention);
    }
*/
}

static int
set_light_battery(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_battery = *state;
    LOGV("set_light_battery color=0x%08x mode=%d", state->color, state->flashMode);
    handle_speaker_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int
set_light_notifications(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_notification = *state;
    LOGV("set_light_notifications color=0x%08x mode=%d", state->color, state->flashMode);
    handle_speaker_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int
set_light_attention(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    LOGV("set_light_attention color=0x%08x mode=%d", state->color, state->flashMode);
    g_attention = *state;
    if(state->flashMode != LIGHT_FLASH_NONE) {
	if(g_attention.color)
	        g_attention.color=0xff0000;
    }
    else {
	if(g_attention.color) {
		g_attention.color=0;
		g_attention.flashMode=LIGHT_FLASH_NONE;
	}
    }
//    handle_speaker_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}


/** Close the lights device */
static int
close_lights(struct light_device_t *dev)
{
    if (dev) {
        free(dev);
    }
    return 0;
}


/******************************************************************************/

/**
 * module methods
 */

/** Open a new instance of a lights device using name */
static int open_lights(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
    int (*set_light)(struct light_device_t* dev,
            struct light_state_t const* state);

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name)) {
        set_light = set_light_backlight;
    }
    else if (0 == strcmp(LIGHT_ID_KEYBOARD, name)) {
        set_light = set_light_keyboard;
    }
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name)) {
        set_light = set_light_buttons;
    }
    else if (0 == strcmp(LIGHT_ID_BATTERY, name)) {
        set_light = set_light_battery;
    }
    else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name)) {
        set_light = set_light_notifications;
    }
    else if (0 == strcmp(LIGHT_ID_ATTENTION, name)) {
        set_light = set_light_attention;
    }
    else {
        return -EINVAL;
    }

    pthread_once(&g_init, init_globals);

    struct light_device_t *dev = malloc(sizeof(struct light_device_t));
    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t*)dev;
    return 0;
}


static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

/*
 * The lights Module
 */
const struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "QCT MSM7K lights Module",
    .author = "Google, Inc.",
    .methods = &lights_module_methods,
};
