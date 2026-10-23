/* Vita implementations of the supported wrapper's platform services.
 * Services absent on Vita use the wrapper/engine's documented failure paths.
 * Never manufacture a purchase, permission grant, location or media playback.
 */
#include "java_platform.h"
#include "sif.h"
#include "fonts.h"
#include "compat.h"
#include "asset_pack.h"
#include "jni_utf.h"
#include <psp2/kernel/clib.h>
#include <psp2/kernel/rng.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ime_dialog.h>
#include <psp2/message_dialog.h>
#include <psp2/sysmodule.h>
#include <psp2/motion.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define SLOT_COUNT 100
struct Widget {
    int used,x,y,w,h,enabled,visible,password,maxlen,alignment,pending,dirty;
    int zoom,alpha,color;
    char *text,*hint;
    jobject paint;
    unsigned texture;
};
static struct Widget textboxes[SLOT_COUNT],webviews[SLOT_COUNT],indicator;
static int ime_slot=-1,message_active,motion_active;
static SceWChar16 ime_initial[2049],ime_output[2049],ime_title[129];
static char alert_pending[512],alert_current[512];
static char *clipboard;
static float theta,phi,gravity[3];
static void (*notification_cb)(JNIEnv*,jobject,jint,jint,jstring);
static void (*location_cb)(JNIEnv*,jobject,jint,jint,jdouble,jdouble,jstring);
static void (*control_cb)(JNIEnv*,jobject,jint,jint,jstring,jstring);
static void (*web_cb)(JNIEnv*,jobject,jobject,jint,jstring);
static void (*share_cb)(JNIEnv*,jobject,jstring,jboolean,jstring);
extern double sif_asin(double);
extern double sif_atan2(double,double);
extern double sif_sqrt(double);

static void *callback(const char *suffix) {
    char name[128];snprintf(name,sizeof(name),"app_klb_android_GameEngine_PFInterface_%s",suffix);
    void *p=(void*)so_symbol(&so_mod,name);if(!p)sif_stop("Platform JNI callback missing");return p;
}
void sif_java_platform_init(void) {
    notification_cb=callback("OnNotificationCallback");location_cb=callback("OnLocationCallback");
    control_cb=callback("clientControlEvent");web_cb=callback("WebViewControlEvent");share_cb=callback("onShareCallback");
}
static char *string_copy(jstring js) {
    if(!js)return strdup("");
    const char *s=jni->GetStringUTFChars(&jni,js,NULL);
    char *p=s?strdup(s):NULL;
    if(s)jni->ReleaseStringUTFChars(&jni,js,(char*)s);
    if(!p)sif_stop("Platform string allocation failed");return p;
}
static void replace(char **dst,jstring js) {char *s=string_copy(js);free(*dst);*dst=s;}
static struct Widget *slot(struct Widget *list,int i) {return i>=0 && i<SLOT_COUNT && list[i].used?&list[i]:NULL;}
static void limit_text(struct Widget *w) {
    if(!w || !w->text || w->maxlen<=0)return;
    size_t bytes=strlen(w->text),count=sif_mutf8_decode((const unsigned char*)w->text,bytes,NULL);
    if(count<=(unsigned)w->maxlen)return;
    uint16_t *chars=malloc(count*2);if(!chars)sif_stop("Text limit allocation failed");
    sif_mutf8_decode((const unsigned char*)w->text,bytes,chars);count=w->maxlen;
    if(count && chars[count-1]>=0xd800 && chars[count-1]<=0xdbff)count--;
    size_t n=sif_mutf8_encode(chars,count,(unsigned char*)w->text);w->text[n]=0;free(chars);w->dirty=1;
}
static int create(struct Widget *list,va_list a,int web) {
    int index=-1;for(int i=0;i<SLOT_COUNT;i++)if(!list[i].used){index=i;break;}
    if(index<0)return -1;
    struct Widget *w=&list[index];memset(w,0,sizeof(*w));
    w->x=va_arg(a,int);w->y=va_arg(a,int);w->w=va_arg(a,int);w->h=va_arg(a,int);
    w->text=string_copy(va_arg(a,jstring));
    if(!web)w->password=va_arg(a,int);
    w->used=w->enabled=w->visible=w->dirty=1;w->maxlen=2048;w->pending=web;
    return index;
}
static int destroy(struct Widget *list,int index) {
    struct Widget *w=slot(list,index);if(!w)return JNI_FALSE;
    if(list==textboxes && ime_slot==index){sceImeDialogAbort();sceImeDialogTerm();ime_slot=-1;}
    free(w->text);free(w->hint);if(w->texture)sif_overlay_delete(w->texture);
    memset(w,0,sizeof(*w));return JNI_TRUE;
}
static void update(struct Widget *w,va_list a) {
    if(!w)return;
    int x=va_arg(a,int),y=va_arg(a,int),width=va_arg(a,int),height=va_arg(a,int);
    if(w->w!=width || w->h!=height)w->dirty=1;
    w->x=x;w->y=y;w->w=width;w->h=height;
    int enabled=va_arg(a,int);if(enabled!=w->enabled)w->dirty=1;w->enabled=enabled;w->visible=va_arg(a,int);
}
jint sif_jp_textbox_create(jmethodID id,va_list a){return create(textboxes,a,0);}
jboolean sif_jp_textbox_destroy(jmethodID id,va_list a){return destroy(textboxes,va_arg(a,int));}
jboolean sif_jp_textbox_setText(jmethodID id,va_list a){struct Widget *w=slot(textboxes,va_arg(a,int));jstring s=va_arg(a,jstring);if(!w)return 0;replace(&w->text,s);limit_text(w);w->dirty=1;return 1;}
jobject sif_jp_textbox_getText(jmethodID id,va_list a){struct Widget *w=slot(textboxes,va_arg(a,int));return w?jni->NewStringUTF(&jni,w->text):NULL;}
void sif_jp_textbox_update(jmethodID id,va_list a){struct Widget *w=slot(textboxes,va_arg(a,int));update(w,a);}
void sif_jp_textbox_setpaint(jmethodID id,va_list a){struct Widget *w=slot(textboxes,va_arg(a,int));jobject p=va_arg(a,jobject);if(w){w->paint=p;w->dirty=1;}}
void sif_jp_textbox_sethint(jmethodID id,va_list a){struct Widget *w=slot(textboxes,va_arg(a,int));jstring s=va_arg(a,jstring);if(w){replace(&w->hint,s);w->dirty=1;}}
void sif_jp_textbox_setMaxlen(jmethodID id,va_list a){struct Widget *w=slot(textboxes,va_arg(a,int));int n=va_arg(a,int);if(w){w->maxlen=n<=0||n>2048?2048:n;limit_text(w);}}
void sif_jp_textbox_setAlignment(jmethodID id,va_list a){struct Widget *w=slot(textboxes,va_arg(a,int));int n=va_arg(a,int);if(w){w->alignment=n;w->dirty=1;}}
/* A logical WebView handle is needed for asynchronous FAILEDLOADWEB routing.
 * No network request is issued and no authentication headers are retained.
 */
jint sif_jp_webview_create(jmethodID id,va_list a){sif_log("WebView unavailable; queuing load failure");return create(webviews,a,1);}
jboolean sif_jp_webview_destroy(jmethodID id,va_list a){return destroy(webviews,va_arg(a,int));}
jboolean sif_jp_webview_setText(jmethodID id,va_list a){struct Widget *w=slot(webviews,va_arg(a,int));jstring s=va_arg(a,jstring);if(!w)return 0;replace(&w->text,s);w->pending=1;return 1;}
jint sif_jp_webview_getWebViewItem(jmethodID id,va_list a){void *p=va_arg(a,void*);for(int i=0;i<SLOT_COUNT;i++)if(webviews[i].used && p==&webviews[i])return i;return -1;}
void sif_jp_webview_setZoom(jmethodID id,va_list a){struct Widget *w=slot(webviews,va_arg(a,int));int v=va_arg(a,int);if(w)w->zoom=v;}
void sif_jp_webview_setColor(jmethodID id,va_list a){struct Widget *w=slot(webviews,va_arg(a,int));int alpha=va_arg(a,int),color=va_arg(a,int);if(w){w->alpha=alpha;w->color=color;}}
jobject sif_jp_webview_getText(jmethodID id,va_list a){struct Widget *w=slot(webviews,va_arg(a,int));return w?jni->NewStringUTF(&jni,w->text):NULL;}
jobject sif_jp_webview_getTmpText(jmethodID id,va_list a){return sif_jp_webview_getText(id,a);}
void sif_jp_webview_update(jmethodID id,va_list a){struct Widget *w=slot(webviews,va_arg(a,int));update(w,a);}
/* CAndroidMovieWidget::create rejects negative handles; texture movies use 0
 * for decoder/open failure. No decoder is installed by this bridge. */
jint sif_jp_movieview_create(jmethodID id,va_list a){sif_log("Movie view unavailable: create failed");return -1;}
jboolean sif_jp_movieview_destroy(jmethodID id,va_list a){return 0;}
jboolean sif_jp_movieview_setText(jmethodID id,va_list a){return 0;}
void sif_jp_movieview_update(jmethodID id,va_list a){}
void sif_jp_movieview_cmd(jmethodID id,va_list a){}
jint sif_jp_MovieCreate(jmethodID id,va_list a){sif_log("Movie texture unavailable: open failed");return 0;}
void sif_jp_MovieRelease(jmethodID id,va_list a){}
void sif_jp_MovieRefreshTexture(jmethodID id,va_list a){}
jfloat sif_jp_MovieGetInfo(jmethodID id,va_list a){return 0;}
void sif_jp_MovieSetPlay(jmethodID id,va_list a){}
void sif_jp_MovieSetPause(jmethodID id,va_list a){}
void sif_jp_MovieResetLoop(jmethodID id,va_list a){}
jint sif_jp_indicator_create(jmethodID id,va_list a){if(indicator.used){indicator.visible=1;return -1;}indicator.x=va_arg(a,int);indicator.y=va_arg(a,int);indicator.w=va_arg(a,int);indicator.h=va_arg(a,int);indicator.used=indicator.visible=indicator.enabled=1;return 0;}
jboolean sif_jp_indicator_destroy(jmethodID id,va_list a){if(indicator.texture)sif_overlay_delete(indicator.texture);memset(&indicator,0,sizeof(indicator));return 1;}
void sif_jp_indicator_update(jmethodID id,va_list a){(void)va_arg(a,int);if(indicator.used)update(&indicator,a);}
jobject sif_jp_generateDeviceIdent(jmethodID id,va_list a){unsigned random;char s[96];if(sceKernelGetRandomNumber(&random,sizeof(random))<0)sif_stop("Device identity RNG failed");snprintf(s,sizeof(s),SIF_DEVICE_MANUFACTURER "`" SIF_DEVICE_MODEL "`%u",random&0xffffff);return jni->NewStringUTF(&jni,s);}
static void alert(const char *s){snprintf(alert_pending,sizeof(alert_pending),"%s",s);}
void sif_jp_billingGetProducts(jmethodID id,va_list a){sif_log("Billing unavailable: empty product catalogue");control_cb(&jni,NULL,1,0,jni->NewStringUTF(&jni,"[]"),jni->NewStringUTF(&jni,""));}
void sif_jp_billingBuyItem(jmethodID id,va_list a){jstring sku=va_arg(a,jstring);sif_log("Billing unavailable: purchase failed");control_cb(&jni,NULL,4,0,sku,jni->NewStringUTF(&jni,""));}
void sif_jp_billingConsume(jmethodID id,va_list a){sif_log("Billing unavailable: consume failed; receipt unchanged");}
void sif_jp_startAlertDialog(jmethodID id,va_list a){char *title=string_copy(va_arg(a,jstring)),*message=string_copy(va_arg(a,jstring));snprintf(alert_pending,sizeof(alert_pending),"%s\n%s",title,message);free(title);free(message);}
void sif_jp_startMailer(jmethodID id,va_list a){alert("Email is unavailable in this port.");}
void sif_jp_startBrowser(jmethodID id,va_list a){alert("Web browsing is unavailable in this port.");}
jboolean sif_jp_startMap(jmethodID id,va_list a){return 0;}
void sif_jp_startSettings(jmethodID id,va_list a){alert("Android settings are unavailable on PS Vita.");}
void sif_jp_startApp(jmethodID id,va_list a){alert("Android applications cannot be opened on PS Vita.");}
void sif_jp_clearCookies(jmethodID id,va_list a){/* WebView backend never stores cookies. */}
void sif_jp_setLocalNotificationWithAlarm(jmethodID id,va_list a){sif_log("Local notifications unavailable: alarm not scheduled");}
void sif_jp_cancelLocalNotification(jmethodID id,va_list a){/* No alarms can be registered in this backend. */}
jboolean sif_jp_getEnableNotification(jmethodID id,va_list a){return 0;}
void sif_jp_exitGame(jmethodID id,va_list a){sceKernelExitProcess(1);}
void sif_jp_quitGame(jmethodID id,va_list a){sceKernelExitProcess(0);}
void sif_jp_copyToClipBoard(jmethodID id,va_list a){replace(&clipboard,va_arg(a,jstring));sif_log("Clipboard copied within this process");}
static double free_memory(void){SceKernelFreeMemorySizeInfo m={.size=sizeof(m)};if(sceKernelGetFreeMemorySize(&m)<0)return -1;return m.size_user/1048576.0;}
jdouble sif_jp_getFreeMemorySize(jmethodID id,va_list a){return free_memory();}
jdouble sif_jp_getUsedMemorySize(jmethodID id,va_list a){double free=free_memory();/* Vita has 512 MiB of main RAM, including system reservations. */return free<0?-1:512.0-free;}
void sif_jp_getLocation(jmethodID id,va_list a){location_cb(&jni,NULL,1,1,-1.0,-1.0,jni->NewStringUTF(&jni,"Location unavailable on this platform"));}
jboolean sif_jp_stopGetLocation(jmethodID id,va_list a){return 0;}
void sif_jp_requirePermission(jmethodID id,va_list a){int kind=va_arg(a,int);sif_log("JNI permission request kind=%d",kind);if(kind==1)sif_jp_getLocation(id,a);else if(kind==2)notification_cb(&jni,NULL,1,1,jni->NewStringUTF(&jni,"Notifications unavailable on this platform"));else if(kind==3)alert("Saving to the system photo album is unavailable in this port.");}
jint sif_jp_getPermissionStatus(jmethodID id,va_list a){return 2;/* Location permission denied. */}
/* Restrict recursive removal to descendants of the game's writable roots.
 * Reject traversal and symlinks; never recursively remove a mount or root. */
static int erase_tree(const char *path,unsigned depth){
    struct stat st;if(depth>64)return -1;if(lstat(path,&st))return errno==ENOENT?0:-1;
    if(!S_ISDIR(st.st_mode))return unlink(path);
    DIR *d=opendir(path);if(!d)return -1;struct dirent *e;int result=0;
    while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;char child[1024];int n=snprintf(child,sizeof(child),"%s/%s",path,e->d_name);if(n<0||n>=sizeof(child)||erase_tree(child,depth+1)){result=-1;break;}}
    closedir(d);return result?result:rmdir(path);
}
jboolean sif_jp_eraseFolder(jmethodID id,va_list a){
    char *raw=string_copy(va_arg(a,jstring));struct sif_asset_ref ref;
    const char *p=sif_io_target(raw,&ref);char *path=p?strdup(p):NULL;free(raw);if(!path)return 0;
    size_t n=strlen(path);while(n&&path[n-1]=='/')path[--n]=0;
    /* Only strict descendants of the external tree or other loader data; the
     * install tree and the roots themselves are never erased. */
    int allowed=!strstr(path,"..") && !strchr(path+4,':') && strncmp(path,INSTALL_PATH,strlen(INSTALL_PATH)) &&
        (!strncmp(path,EXTERNAL_PATH,strlen(EXTERNAL_PATH)) || (!strncmp(path,DATA_PATH,strlen(DATA_PATH)) && ref.root<0));
    struct sif_pack_stat st;
    if(allowed && sif_pack_stat(&ref,&st) && sif_pack_before_write(&ref,SIF_PACK_REMOVE)<0)allowed=0;
    int r=allowed && erase_tree(path,0)==0;free(path);return r;
}
void sif_jp_shareContents(jmethodID id,va_list a){jstring cb=va_arg(a,jstring);share_cb(&jni,NULL,cb,JNI_FALSE,jni->NewStringUTF(&jni,"Unavailable"));}
void sif_jp_savePng2Album(jmethodID id,va_list a){alert("Saving to the system photo album is unavailable in this port.");}
void sif_jp_startObservation(jmethodID id,va_list a){if(!motion_active){int r=sceMotionStartSampling();motion_active=r>=0||r==SCE_MOTION_ERROR_ALREADY_SAMPLING;if(!motion_active)sif_log("Motion unavailable: %08x",r);}}
void sif_jp_stopObservation(jmethodID id,va_list a){if(motion_active)sceMotionStopSampling();motion_active=0;}
jfloat sif_jp_getTheta(jmethodID id,va_list a){return theta;}
jfloat sif_jp_getPhi(jmethodID id,va_list a){return phi;}

int sif_java_platform_modal(void){return ime_slot>=0 || message_active;}
static void to_utf16(const char *s,SceWChar16 *out,unsigned capacity){
    s=s?s:"";size_t bytes=strlen(s),count=sif_mutf8_decode((const unsigned char*)s,bytes,NULL);
    uint16_t *chars=malloc((count+1)*2);if(!chars)sif_stop("UTF16 allocation failed");
    sif_mutf8_decode((const unsigned char*)s,bytes,chars);if(count>=capacity)count=capacity-1;
    if(count && chars[count-1]>=0xd800 && chars[count-1]<=0xdbff)count--;
    memcpy(out,chars,count*2);out[count]=0;free(chars);
}
int sif_java_platform_touch(int x,int y){
    if(sif_java_platform_modal())return 1;
    for(int i=SLOT_COUNT-1;i>=0;i--){struct Widget *w=&textboxes[i];if(!w->used||!w->enabled||!w->visible||x<w->x||y<w->y||x>=w->x+w->w||y>=w->y+w->h)continue;
        if(sceSysmoduleIsLoaded(SCE_SYSMODULE_IME)<0 && sceSysmoduleLoadModule(SCE_SYSMODULE_IME)<0){sif_log("IME module unavailable");return 1;}
        to_utf16(w->text,ime_initial,w->maxlen+1);to_utf16(w->hint,ime_title,129);memset(ime_output,0,sizeof(ime_output));
        SceImeDialogParam p;sceImeDialogParamInit(&p);p.type=SCE_IME_TYPE_DEFAULT;p.dialogMode=SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
        p.textBoxMode=w->password?SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD:SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
        p.title=ime_title;p.maxTextLength=w->maxlen;p.initialText=ime_initial;p.inputTextBuffer=ime_output;
        int r=sceImeDialogInit(&p);if(r>=0)ime_slot=i;else sif_log("IME init failed: %08x",r);return 1;
    }return 0;
}
void sif_java_platform_frame(void){
    if(ime_slot>=0 && sceImeDialogGetStatus()==SCE_COMMON_DIALOG_STATUS_FINISHED){SceImeDialogResult r={0};int slot_id=ime_slot;ime_slot=-1;if(sceImeDialogGetResult(&r)>=0 && r.button==SCE_IME_DIALOG_BUTTON_ENTER){unsigned n=0;while(n<2048 && ime_output[n])n++;struct Widget *w=slot(textboxes,slot_id);if(w){jstring js=jni->NewString(&jni,ime_output,n);replace(&w->text,js);w->dirty=1;}}sceImeDialogTerm();}
    if(message_active && sceMsgDialogGetStatus()==SCE_COMMON_DIALOG_STATUS_FINISHED){sceMsgDialogTerm();message_active=0;}
    if(!sif_java_platform_modal() && alert_pending[0]){snprintf(alert_current,sizeof(alert_current),"%s",alert_pending);alert_pending[0]=0;SceMsgDialogUserMessageParam u={.buttonType=SCE_MSG_DIALOG_BUTTON_TYPE_OK,.msg=alert_current};SceMsgDialogParam p;sceMsgDialogParamInit(&p);p.mode=SCE_MSG_DIALOG_MODE_USER_MSG;p.userMsgParam=&u;int r=sceMsgDialogInit(&p);if(r>=0)message_active=1;else sif_log("Message dialog failed: %08x",r);}
    for(int i=0;i<SLOT_COUNT;i++)if(webviews[i].used && webviews[i].pending){webviews[i].pending=0;web_cb(&jni,NULL,&webviews[i],2,jni->NewStringUTF(&jni,webviews[i].text));}
    if(motion_active){SceMotionState m;if(sceMotionGetState(&m)>=0){float v[3]={m.acceleration.x,m.acceleration.y,m.acceleration.z};double sum=0;for(int i=0;i<3;i++){gravity[i]=gravity[i]*0.9f+v[i]*0.1f;sum+=(double)gravity[i]*gravity[i];}if(sum>0.000001){double norm=sif_sqrt(sum);theta=sif_asin(-gravity[1]/norm);phi=sif_atan2(-gravity[0],gravity[2]);}}}
}
void sif_java_platform_draw(void){
    int begun=0;
    for(int i=0;i<SLOT_COUNT;i++){struct Widget *w=&textboxes[i];if(!w->used||!w->visible||w->w<=0||w->h<=0||w->w>2048||w->h>2048)continue;
        if(!begun){sif_overlay_begin();begun=1;}
        unsigned *pixels=NULL;
        if(w->dirty||!w->texture){pixels=calloc((size_t)w->w*w->h,4);if(!pixels)sif_stop("Textbox bitmap allocation failed");for(int y=0;y<w->h;y++)for(int x=0;x<w->w;x++)pixels[y*w->w+x]=(x<3||y<3||x>=w->w-3||y>=w->h-3)?0xff9c61ff:0xffffffff;
            const char *label=w->text[0]?w->text:(w->hint?w->hint:"");char mask[2049];if(w->password&&w->text[0]){unsigned n=sif_mutf8_decode((const unsigned char*)w->text,strlen(w->text),NULL);if(n>2048)n=2048;memset(mask,'*',n);mask[n]=0;label=mask;}
            sif_font_widget(w->paint,label,pixels,w->w,w->h,w->alignment,w->enabled);w->dirty=0;
        }
        sif_overlay_bitmap(&w->texture,pixels,w->w,w->h,w->x,w->y);free(pixels);
    }
    if(indicator.used&&indicator.visible){if(!begun){sif_overlay_begin();begun=1;}unsigned pixels[32*32]={0};unsigned tick=sceKernelGetProcessTimeWide()/100000;for(int y=0;y<32;y++)for(int x=0;x<32;x++){int dx=x-16,dy=y-16,d=dx*dx+dy*dy;if(d>80&&d<180)pixels[y*32+x]=((x+tick)%16<8)?0xff9c61ff:0xffdddddd;}sif_overlay_bitmap(&indicator.texture,pixels,32,32,indicator.x,indicator.y);}
    if(begun)sif_overlay_end();
}
