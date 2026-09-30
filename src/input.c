#include "sif.h"
#include "java_platform.h"
#include <falso_jni/FalsoJNI.h>
#include <psp2/touch.h>
#include <string.h>
static void (*input_point)(JNIEnv*,jobject,jint,jint,jint,jint);
static SceTouchPanelInfo panel;
static struct {int active,id,x,y,captured;} contacts[6];
void sif_input_init(void) {
    input_point=(void*)so_symbol(&so_mod,"app_klb_android_GameEngine_PFInterface_inputPoint");
    if(!input_point)sif_stop("Touch entrypoint missing");
    if(sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT,&panel)<0 || panel.maxDispX<=panel.minDispX || panel.maxDispY<=panel.minDispY)
        sif_stop("Touch panel unavailable");
    if(sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START)<0)sif_stop("Touch sampling failed");
}
void sif_input_poll(void) {
    SceTouchData d;if(sceTouchPeek(SCE_TOUCH_PORT_FRONT,&d,1)<0)return;
    int present[6]={0};
    if(sif_java_platform_modal())for(int slot=0;slot<6;slot++)
        if(contacts[slot].active && !contacts[slot].captured){input_point(&jni,NULL,slot,2,contacts[slot].x,contacts[slot].y);contacts[slot].captured=1;}
    // Release vanished contacts before reusing their game pointer IDs.
    for(int slot=0;slot<6;slot++)if(contacts[slot].active) {
        for(unsigned k=0;k<d.reportNum;k++)if(d.report[k].id==contacts[slot].id)present[slot]=1;
        if(!present[slot]) {if(!contacts[slot].captured)input_point(&jni,NULL,slot,2,contacts[slot].x,contacts[slot].y);contacts[slot].active=0;}
    }
    for(unsigned k=0;k<d.reportNum;k++) {
        SceTouchReport *r=&d.report[k];int slot=-1,fresh=0;
        for(int j=0;j<6;j++)if(contacts[j].active && contacts[j].id==r->id)slot=j;
        if(slot<0)for(int j=0;j<6;j++)if(!contacts[j].active){slot=j;fresh=1;break;}
        if(slot<0)sif_stop("Touch pointer capacity exceeded");
        int x=(r->x-panel.minDispX)*960/(panel.maxDispX-panel.minDispX+1);
        int y=(r->y-panel.minDispY)*544/(panel.maxDispY-panel.minDispY+1);
        if(x<0)x=0;if(x>959)x=959;if(y<0)y=0;if(y>543)y=543;
        if(fresh)contacts[slot].captured=sif_java_platform_touch(x,y);
        if(!contacts[slot].captured && (fresh || x!=contacts[slot].x || y!=contacts[slot].y))input_point(&jni,NULL,slot,fresh?0:1,x,y);
        contacts[slot].active=1;contacts[slot].id=r->id;contacts[slot].x=x;contacts[slot].y=y;
    }
}
void sif_input_release(void) {
    for(int slot=0;slot<6;slot++)if(contacts[slot].active) {
        if(!contacts[slot].captured)input_point(&jni,NULL,slot,2,contacts[slot].x,contacts[slot].y);contacts[slot].active=0;
    }
}
