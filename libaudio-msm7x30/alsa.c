/*
 *            libaudioalsa for MSM7x30
 *            for some reason Qualcomm seems to think that this is proprietary
 *            but it's just a standard ALSA interface to audio routing
 *                           Martin Johnson 5/7/2011
 *            License: GPL
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "asound.h"
#define LOG_TAG "LibAudioAlsa"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#define MAX_DEVICES 64

// first 9 mixer controls are not passed to libaudio
// they are used for routing and other config
// this means if the no of devices changes the code may break
#define NO_OF_CTRLS 9


struct mixer_device_t {
    int id;
    char name[64];
    int class_id;
    int capability;
    int elem_id;
};

static int device_count;

static char *device_list[MAX_DEVICES];

static struct mixer_device_t mixer_device[MAX_DEVICES];

enum ctrls {COUNT,STREAM,RECORD,VOICE,VOLUME,VOICEVOLUME,VOICEMUTE,VOICECALL,DEVICEVOLUME};
static char *ctrl_names[NO_OF_CTRLS]={
    "Count","Stream","Record","Voice",
    "Volume","VoiceVolume","VoiceMute",
    "Voice Call","Device_Volume"
};

static int ctrls[NO_OF_CTRLS];

static int mixerfd;

static int mixercount;

int msm_mixer_count() {
    int ret;
    struct snd_ctl_elem_list elist;
    elist.offset=0;
    elist.space=0;

    ret=ioctl(mixerfd,SNDRV_CTL_IOCTL_ELEM_LIST,&elist);
    if (ret<0)
        LOGE("get elist failed %d %d",ret, errno);

    return elist.count;
}

void elem_write(int id,int v,int v1,int v2) {
    struct snd_ctl_elem_value elval;
    int ret;

    elval.id.numid=id;
    elval.indirect=0;
    elval.value.integer.value[0]=v;
    elval.value.integer.value[1]=v1;
    elval.value.integer.value[2]=v2;
    ret=ioctl(mixerfd,SNDRV_CTL_IOCTL_ELEM_WRITE,&elval);
    if (ret<0)
        LOGE("elem_write failed %d", errno);
}



int msm_mixer_open(const char *name,int card) {
    int n,m,i,j;
    struct snd_ctl_elem_id *eid = NULL;
    struct snd_ctl_elem_list elist;
    struct snd_ctl_elem_info einfo;
    struct snd_ctl_elem_value elval;
    int ret;

    mixerfd=open(name, O_RDWR, 0);
    LOGI("opened mixer %d %s %d",mixerfd,name,card);
    mixercount=msm_mixer_count();
    elist.offset=0;
    elist.space=mixercount;
    elist.pids=malloc(sizeof(struct snd_ctl_elem_id)*elist.space);
    device_count=0;
    ret=ioctl(mixerfd,SNDRV_CTL_IOCTL_ELEM_LIST,&elist);
    if (ret<0)
        LOGE("get elist failed %d %d",ret, errno);
    LOGD("got elist %d", elist.count);
    for (i=0;i<mixercount;i++) {
        LOGD("id %d name %s: %d %d %d %x",elist.pids[i].numid,elist.pids[i].name, elist.pids[i].device, elist.pids[i].subdevice, elist.pids[i].index ,elist.pids[i].iface);
        einfo.id.numid=elist.pids[i].numid;
        ret=ioctl(mixerfd,SNDRV_CTL_IOCTL_ELEM_INFO,&einfo);
        if (ret<0)
            LOGE("get einfo failed %d %d",ret, errno);
        LOGD("einfo type %d %d %d %ld %ld",einfo.type, einfo.access, einfo.count, einfo.value.integer.min, einfo.value.integer.max);
        elval.id.numid=elist.pids[i].numid;
        elval.indirect=0;
        elval.value.integer.value[0]=0;
        elval.value.integer.value[1]=0;
        elval.value.integer.value[2]=0;
        ret=ioctl(mixerfd,SNDRV_CTL_IOCTL_ELEM_READ,&elval);
        if (ret<0)
            LOGE("get eval failed %d %d",ret, errno);
        LOGD("eval %ld %ld %ld",elval.value.integer.value[0],elval.value.integer.value[1],elval.value.integer.value[2]);
        for (j=0;j<NO_OF_CTRLS;j++)
            if (!strcmp(ctrl_names[j],(const char *)elist.pids[i].name))
                ctrls[j]=elist.pids[i].numid;
        if (i>=NO_OF_CTRLS) {
            mixer_device[device_count].id=device_count;
            mixer_device[device_count].elem_id=elist.pids[i].numid;
            mixer_device[device_count].class_id=elval.value.integer.value[0];
            mixer_device[device_count].capability=elval.value.integer.value[1];
            strcpy(mixer_device[device_count].name, (const char *)elist.pids[i].name);
            device_list[device_count]=mixer_device[device_count].name;
            device_count++;
        }
    }
    for (i=0;i<NO_OF_CTRLS;i++)
        LOGD("ctrl %s=%d",ctrl_names[i],ctrls[i]);
    free(elist.pids);
    return 0;

}

int msm_mixer_close() {
    close(mixerfd);
    return 0;
}
int msm_get_device(const char *name) {
    int i;
    for (i=0;i<device_count;i++)
        if (!strcmp(mixer_device[i].name,name))
            return mixer_device[i].id;
    return 0;
}
int msm_en_device(int dev_id,int set) {
    LOGD("Enable device %d %d", dev_id,set);
    elem_write(mixer_device[dev_id].elem_id,set,0,0);
    return 0;
}
int msm_route_stream(int dir, int dec_id, int dev_id, int set) {
    LOGD("Route Stream %d %d %d %d",dir,dec_id,dev_id,set);
    if (dir==1) // PCM_PLAY
        elem_write(ctrls[STREAM],dec_id,dev_id,set);
    else
        elem_write(ctrls[RECORD],dec_id,dev_id,set);
    return 0;
}
int msm_route_voice(int rx,int tx,int set) {
    LOGD("Route Voice %d %d %d",tx,rx,set);
    elem_write(ctrls[VOICE],rx,tx,set);  // tx and rx seem to be swapped here!
    return 0;
}
int msm_set_volume(int dec_id, int vol) {
    LOGD("Set Volume %d %d",dec_id,vol);
    elem_write(ctrls[VOLUME], dec_id, vol, 0);
    return 0;
}
int msm_get_device_class(int device_id) {
    return mixer_device[device_id].class_id;
}
int msm_get_device_capability(int device_id) {
    return mixer_device[device_id].capability;
}

char **msm_get_device_list() {
    return device_list;
}
int msm_get_device_count() {
    return device_count;
}
int msm_start_voice() {
    LOGD("Start Voice");
    elem_write(ctrls[VOICECALL], 1,0,0);
    return 0;
}
int msm_end_voice() {
    LOGD("End Voice");
    elem_write(ctrls[VOICECALL], 0,0,0);
    return 0;
}
int msm_set_voice_tx_mute(int mute) {
    LOGD("Set Voice tx Mute %d",mute);
    elem_write(ctrls[VOICEMUTE], 2,mute,0); // 2 is DIR_TX
    return 0;
}
int msm_set_voice_rx_vol(int volume) {
    LOGD("Set Voice rx Volume %d",volume);
    elem_write(ctrls[VOICEVOLUME], 1, volume,0);
    return 0;
}
int msm_set_device_volume(int dev_id,int volume) {
    LOGD("Set Device Volume %d %d",dev_id, volume);
    elem_write(ctrls[DEVICEVOLUME], dev_id, volume,0);
    return 0;
}
int msm_reset_all_device() {
    return 0;
}

