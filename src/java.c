#include <falso_jni/FalsoJNI.h>
#include "sif.h"
#include "fonts.h"
#include "crypto.h"
#include "java_platform.h"
#include <psp2/kernel/rng.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/rtc.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>

jobject sif_keychain_get(jmethodID,va_list);
jboolean sif_keychain_set(jmethodID,va_list);
jboolean sif_keychain_delete(jmethodID,va_list);
/*
 * JNI Methods
*/

static jint version(jmethodID id,va_list a) { return SIF_ANDROID_SDK; }
static jint audio_hz(jmethodID id,va_list a) { return 44100; }
static jint audio_samples(jmethodID id,va_list a) { return 256; }
static jint android_debug_setting(jmethodID id,va_list a) {
    // PFInterface reads adb_enabled, development_settings_enabled and
    // mock_location through Settings.Secure.getInt(..., 0). None of these
    // Android settings/services exists in the Vita compatibility environment.
    return 0;
}
// Initial game locale is Japanese; preserve the supplied wrapper's package
// suffix rule and the exact APK version rather than another release's identity.
static jobject app_string(jmethodID id,va_list a) {
    const char *s;
    switch((uintptr_t)id) {
        case 10:s="com.npdep.wrapperthingen_jp";break;
        case 11:s="9.11";break;
        case 12:s="ja";break;case 13:s="JP";break;case 14:s="ja_JP";break;
        case 43:s=SIF_ANDROID_RELEASE;break;
        case 44:s=SIF_DEVICE_PRODUCT;break;case 45:s=SIF_DEVICE_MANUFACTURER;break;
        case 46:s=SIF_DEVICE_BRAND;break;case 47:s=SIF_DEVICE_DEVICE;break;
        case 48:s=SIF_DEVICE_MODEL;break;case 49:s=SIF_DEVICE_BOARD;break;
        case 50:s=SIF_BUILD_FINGERPRINT;break;case 51:s=SIF_BUILD_TAGS;break;
        // Android strips "/install/" from its asset directory to return
        // the writable application base, without a trailing slash. Vita's
        // asset roots are selected independently; this base stays stable.
        case 54:s=BASE_PATH;break;
        default:sif_stop("Unknown application string");
    }
    return jni->NewStringUTF(&jni,s);
}
static jlong nanotime(jmethodID id,va_list a) {return (jlong)sceKernelGetProcessTimeWide()*1000;}
static jobject gen_user_id(jmethodID id,va_list args) {
    unsigned char bytes[16];char text[37];
    if(sceKernelGetRandomNumber(bytes,sizeof(bytes))<0)sif_stop("UUID secure RNG failed");
    bytes[6]=(bytes[6]&15)|0x40;bytes[8]=(bytes[8]&63)|0x80;
    unsigned out=0;
    for(unsigned i=0;i<16;i++) {
        if(i==4 || i==6 || i==8 || i==10)text[out++]='-';
        snprintf(text+out,3,"%02x",bytes[i]);out+=2;
    }
    return jni->NewStringUTF(&jni,text);
}
static unsigned keep_screen_on;
static void forbid_sleep(jmethodID id,va_list args) {
    unsigned enabled=va_arg(args,int)!=0;
    __atomic_store_n(&keep_screen_on,enabled,__ATOMIC_RELAXED);
    if(enabled && sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT)<0)
        sif_stop("Keep-screen-on power tick failed");
    sif_log("JNI keep screen on=%u",enabled);
}
void sif_java_power_tick(void) {
    static uint64_t last;
    uint64_t now=sceKernelGetProcessTimeWide();
    if(__atomic_load_n(&keep_screen_on,__ATOMIC_RELAXED) && now-last>=1000000) {
        if(sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT)<0)sif_stop("Keep-screen-on power tick failed");
        last=now;
    }
}
static jdouble unix_time_now(jmethodID id,va_list a) {
    SceRtcTick now,epoch;SceDateTime start={1970,1,1,0,0,0,0};
    if(sceRtcGetCurrentTick(&now)<0 || sceRtcGetTick(&start,&epoch)<0)
        sif_stop("UNIX time RTC conversion failed");
    // Match System.currentTimeMillis()/1000.0, including millisecond precision.
    return (jdouble)((int64_t)(now.tick-epoch.tick)/1000)/1000.0;
}
static jobject date_time_now(jmethodID id,va_list a) {
    // PFInterface uses yyyy-MM-dd-HH-mm-ss in Asia/Tokyo, regardless of
    // the device's local timezone. Convert UTC RTC ticks after adding 9h.
    SceRtcTick utc,tokyo;SceDateTime date;char text[32];
    if(sceRtcGetCurrentTick(&utc)<0 || sceRtcTickAddHours(&tokyo,&utc,9)<0 ||
       sceRtcSetTick(&date,&tokyo)<0)sif_stop("Date/time RTC conversion failed");
    int n=snprintf(text,sizeof(text),"%04u-%02u-%02u-%02u-%02u-%02u",
        (unsigned)date.year,(unsigned)date.month,(unsigned)date.day,
        (unsigned)date.hour,(unsigned)date.minute,(unsigned)date.second);
    if(n<0 || n>=(int)sizeof(text))sif_stop("Date/time formatting failed");
    static unsigned calls;
    if(calls++<2)sif_log("JNI Tokyo date/time: %s",text);
    return jni->NewStringUTF(&jni,text);
}
static char *method_signatures[256];
void sif_java_bind_method(jmethodID id,const char *signature) {
    unsigned i=(uintptr_t)id;if(!i || i>=256)sif_stop("JNI method ID out of range");
    if(method_signatures[i]) {
        if(strcmp(method_signatures[i],signature))sif_stop("JNI method signature changed");
        return;
    }
    method_signatures[i]=strdup(signature);
    if(!method_signatures[i])sif_stop("JNI signature allocation failed");
}
void sif_java_pack_args(jmethodID id,const jvalue *args,void *storage,size_t capacity,va_list *result) {
    _Static_assert(sizeof(va_list)==sizeof(void*),"ARM AAPCS va_list representation");
    unsigned method=(uintptr_t)id;
    if(method>=256 || !method_signatures[method])sif_stop("JNI argument signature unavailable");
    const char *s=method_signatures[method];if(*s++!='(')sif_stop("Invalid JNI signature");
    unsigned index=0;size_t used=0;
    while(*s && *s!=')') {
        if(!args || index>=32)sif_stop("JNI argument array unavailable or too long");
        const jvalue *v=&args[index++];uint64_t wide=0;unsigned width=4;int32_t narrow=0;
        char type=*s++;
        switch(type) {
            case 'Z':narrow=v->z;break;case 'B':narrow=v->b;break;case 'C':narrow=v->c;break;
            case 'S':narrow=v->s;break;case 'I':narrow=v->i;break;
            case 'J':wide=v->j;width=8;break;
            case 'D':memcpy(&wide,&v->d,8);width=8;break;
            case 'F':{double promoted=v->f;memcpy(&wide,&promoted,8);width=8;break;}
            case '[':
                while(*s=='[')s++;
                if(*s=='L') {while(*s && *s!=';')s++;if(!*s)sif_stop("Malformed JNI array type");}
                if(!*s)sif_stop("Malformed JNI array type");s++;
                narrow=(uintptr_t)v->l;break;
            case 'L':while(*s && *s!=';')s++;if(!*s)sif_stop("Malformed JNI object type");s++;narrow=(uintptr_t)v->l;break;
            default:sif_stop("Unsupported JNI argument type");
        }
        used=(used+width-1)&~(size_t)(width-1);
        if(used+width>capacity)sif_stop("JNI argument buffer overflow");
        memcpy((char*)storage+used,width==8?(void*)&wide:(void*)&narrow,width);used+=width;
    }
    if(*s!=')')sif_stop("Malformed JNI argument list");
    memcpy(result,&storage,sizeof(storage));
    static unsigned logged[8];
    if(!(logged[method/32]&(1u<<(method%32)))) {logged[method/32]|=1u<<(method%32);sif_log("JNI array arguments id=%u count=%u",method,index);}
}
static jobject device_token(jmethodID id,va_list a) {
    char token[192]; FILE *f=fopen(DATA_PATH "device-token","rb");
    if(f) { size_t n=fread(token,1,191,f);fclose(f); if(n!=191)sif_stop("Invalid persisted device token"); token[191]=0; }
    else {
        unsigned char random[16],digest[64];char uuid[37];
        if(sceKernelGetRandomNumber(random,sizeof(random))<0)sif_stop("Device token randomness failed");
        random[6]=(random[6]&15)|64;random[8]=(random[8]&63)|128;
        snprintf(uuid,sizeof(uuid),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        random[0],random[1],random[2],random[3],random[4],random[5],random[6],random[7],random[8],random[9],random[10],random[11],random[12],random[13],random[14],random[15]);
        SHA512((unsigned char*)uuid,36,digest);
        for(int i=0;i<64;i++)snprintf(token+i*3,4,i==63?"%02X":"%02X:",digest[i]);
        f=fopen(DATA_PATH "device-token.tmp","wb");if(!f)sif_stop("Token save failed");
        if(fwrite(token,1,191,f)!=191 || fclose(f))sif_stop("Token write failed");
        if(rename(DATA_PATH "device-token.tmp",DATA_PATH "device-token"))sif_stop("Token finalize failed");
    }
    return jni->NewStringUTF(&jni,token);
}
// The supplied Backtrace stockAdditionalInfo only performs optional logging.
// Java string concatenation accepts null; it has no gameplay side effects.
static void stock_info(jmethodID id,va_list a) {
    jstring key=va_arg(a,jstring),value=va_arg(a,jstring);
    const char *k=key?jni->GetStringUTFChars(&jni,key,NULL):NULL;
    const char *v=value?jni->GetStringUTFChars(&jni,value,NULL):NULL;
    if((key && !k) || (value && !v))sif_stop("Backtrace string conversion failed");
    sif_log("Backtrace attribute: %.128s",k?k:"null");
    if(k)jni->ReleaseStringUTFChars(&jni,key,(char*)k);
    if(v)jni->ReleaseStringUTFChars(&jni,value,(char*)v);
}
static void extension_event(jmethodID id,va_list args) {
    jstring name=va_arg(args,jstring),event=va_arg(args,jstring),payload=va_arg(args,jstring);
    const char *n=name?jni->GetStringUTFChars(&jni,name,NULL):NULL;
    const char *e=event?jni->GetStringUTFChars(&jni,event,NULL):NULL;
    if((name && !n) || (event && !e))sif_stop("Extension event string conversion failed");
    // This APK registers Backtrace and Firebase. Only Firebase implements this
    // callback, and its entire method is an Android debug-log statement.
    sif_log("JNI extension event %.96s / %.96s",n?n:"null",e?e:"null");
    if(n)jni->ReleaseStringUTFChars(&jni,name,(char*)n);
    if(e)jni->ReleaseStringUTFChars(&jni,event,(char*)e);
    (void)payload;
}
static unsigned rewarded_ad_status;
static void preload_rewarded_ad(jmethodID id,va_list args) {
    int enabled=va_arg(args,int);
    jstring parameter=va_arg(args,jstring);
    // This APK's Firebase wrapper only reads the ad-unit string, sets
    // PRELOADING, and logs. It contains no ad load or native callback.
    if(__atomic_exchange_n(&rewarded_ad_status,1,__ATOMIC_RELAXED)==1)return;
    sif_log("JNI rewarded-ad preload status=1 enabled=%d",enabled!=0);
    (void)parameter;
}
static void show_rewarded_ad(jmethodID id,va_list args) {
    // Match the supplied wrapper: only LOADED (2) would produce a log.
    // Nothing in that wrapper transitions to LOADED or grants a reward.
    if(__atomic_load_n(&rewarded_ad_status,__ATOMIC_RELAXED)==2)
        sif_log("JNI rewarded-ad show");
    (void)id;(void)args;
}
static void backtrace_message(jmethodID id,va_list a) {
    jstring text=va_arg(a,jstring);const char *s=text?jni->GetStringUTFChars(&jni,text,NULL):NULL;
    if(text && !s)sif_stop("Backtrace message conversion failed");
    const char *name=(uintptr_t)id==31?"breadcrumb":(uintptr_t)id==32?"Lua dump":(uintptr_t)id==33?"exception":"assertion";
    sif_log("Backtrace %s: %.256s",name,s?s:"null");
    if(s)jni->ReleaseStringUTFChars(&jni,text,(char*)s);
}
void sif_java_check_method(const char *name,const char *signature) {
    static const struct {const char *name,*signature;} expected[]={
#define JNI_PLATFORM(id,name,sig,type,ret,table) {#name,sig},
#include "java_platform_methods.inc"
#undef JNI_PLATFORM
        {"getVersionSDK","()I"},{"getOptimalAudioHz","()I"},{"getOptimalAudioSamples","()I"},
        {"getDeviceToken","()Ljava/lang/String;"},
        {"getRandomBytes","([B)Z"},
        {"getDateTimeNow","()Ljava/lang/String;"},{"getUNIXTimeNow","()D"},{"forbidSleep","(Z)V"},{"onExtensionEventRequest","(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V"},
        {"PreloadRewardedAd","(ZLjava/lang/String;)V"},{"ShowRewardedAd","()V"},
        {"getAPKSignature","()Ljava/lang/String;"},
        {"getString","()Ljava/lang/String;"},
        {"sha512","(Ljava/lang/String;)[B"},{"publicKeyVerify","([B[B[B)Z"},{"publicKeyEncrypt","([B[B[B)I"},{"encryptAES128CBC","([B[B[B)I"},{"decryptAES128CBC","([B[B[B)I"},
        {"getBasePath","()Ljava/lang/String;"},
        {"getMode","()I"},{"getOption","()I"},{"genUserID","()Ljava/lang/String;"},
        {"getKeyChain","(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"},
        {"setKeyChain","(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z"},
        {"delKeyChain","(Ljava/lang/String;Ljava/lang/String;)Z"},
        {"getVersionRelease","()Ljava/lang/String;"},
        {"getProductName","()Ljava/lang/String;"},{"getManufacturer","()Ljava/lang/String;"},
        {"getBrand","()Ljava/lang/String;"},{"getDevice","()Ljava/lang/String;"},
        {"getModel","()Ljava/lang/String;"},{"getBoard","()Ljava/lang/String;"},
        {"getFingerPrint","()Ljava/lang/String;"},{"getTags","()Ljava/lang/String;"},
        {"stockAdditionalInfo","(Ljava/lang/String;Ljava/lang/String;)V"},
        {"getPackageName","()Ljava/lang/String;"},{"getVersionName","()Ljava/lang/String;"},
        {"getLanguageCodeRAW","()Ljava/lang/String;"},{"getCountryCodeRAW","()Ljava/lang/String;"},
        {"getPreferredLangCodeRAW","()Ljava/lang/String;"},{"nanotime","()J"},
        {"leaveBreadcrumb","(Ljava/lang/String;)V"},{"sendLuaDump","(Ljava/lang/String;)V"},
        {"sendException","(Ljava/lang/String;)V"},{"beforeAssertFunction","(Ljava/lang/String;)V"},
        {"CreatePaint","(Ljava/lang/String;Z)Landroid/graphics/Paint;"},
        {"getFontMetrics","(Landroid/graphics/Paint;)Landroid/graphics/Paint$FontMetrics;"},
        {"setTextSize","(Landroid/graphics/Paint;F)V"},{"setAntiAlias","(Landroid/graphics/Paint;Z)V"},
        {"measureText","(Landroid/graphics/Paint;Ljava/lang/String;)F"},
        {"drawText","(Landroid/graphics/Paint;[IIIIFFLjava/lang/String;)V"},
    };
    _Static_assert(sizeof(expected)/sizeof(expected[0])<=256,"JNI method audit bitset capacity");
    static unsigned seen[8];
    for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);i++)if(!strcmp(name,expected[i].name)) {
        if(strcmp(signature,expected[i].signature)) {
            sif_log("JNI signature mismatch %s: %s expected %s",name,signature,expected[i].signature);
            sif_stop("JNI signature mismatch");
        }
        if(!(seen[i/32]&(1u<<(i%32)))) {seen[i/32]|=1u<<(i%32);sif_log("JNI method %s %s",name,signature);}
        return;
    }
}
void sif_java_check_field(const char *name,const char *signature) {
    static const struct {const char *name,*signature;} expected[]={
        {"m_path_install","Ljava/lang/String;"},{"m_path_external","Ljava/lang/String;"},
        {"SDK_INT","I"},{"WINDOW_SERVICE","Ljava/lang/String;"},
        {"ascent","F"},{"descent","F"},{"top","F"},{"bottom","F"},{"leading","F"},
    };
    static unsigned seen;
    for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);i++)if(!strcmp(name,expected[i].name) && !strcmp(signature,expected[i].signature)) {
        if(!(seen&(1u<<i))) {seen|=1u<<i;sif_log("JNI field %s %s",name,signature);}
        return;
    }
    sif_log("Unsupported JNI field %s %s",name,signature);
    sif_stop("JNI field unimplemented or signature mismatch");
}
NameToMethodID nameToMethodId[] = {
#define JNI_PLATFORM(id,name,sig,type,ret,table) {id,#name,METHOD_TYPE_##type},
#include "java_platform_methods.inc"
#undef JNI_PLATFORM
 {68,"PreloadRewardedAd",METHOD_TYPE_VOID},{69,"ShowRewardedAd",METHOD_TYPE_VOID},
 {67,"onExtensionEventRequest",METHOD_TYPE_VOID},
 {66,"forbidSleep",METHOD_TYPE_VOID},
 {65,"getUNIXTimeNow",METHOD_TYPE_DOUBLE},
 {63,"encryptAES128CBC",METHOD_TYPE_INT},{64,"decryptAES128CBC",METHOD_TYPE_INT},
 {62,"publicKeyEncrypt",METHOD_TYPE_INT},
 {61,"publicKeyVerify",METHOD_TYPE_BOOLEAN},
 {60,"genUserID",METHOD_TYPE_OBJECT},
 {57,"getKeyChain",METHOD_TYPE_OBJECT},{58,"setKeyChain",METHOD_TYPE_BOOLEAN},{59,"delKeyChain",METHOD_TYPE_BOOLEAN},
 {55,"getMode",METHOD_TYPE_INT},{56,"getOption",METHOD_TYPE_INT},
 {54,"getBasePath",METHOD_TYPE_OBJECT},
 {53,"sha512",METHOD_TYPE_OBJECT},
 {52,"getString",METHOD_TYPE_OBJECT},
 {50,"getFingerPrint",METHOD_TYPE_OBJECT},{51,"getTags",METHOD_TYPE_OBJECT},
 {44,"getProductName",METHOD_TYPE_OBJECT},{45,"getManufacturer",METHOD_TYPE_OBJECT},
 {46,"getBrand",METHOD_TYPE_OBJECT},{47,"getDevice",METHOD_TYPE_OBJECT},
 {48,"getModel",METHOD_TYPE_OBJECT},{49,"getBoard",METHOD_TYPE_OBJECT},
 {43,"getVersionRelease",METHOD_TYPE_OBJECT},
 {42,"getAPKSignature",METHOD_TYPE_OBJECT},
 {41,"getDateTimeNow",METHOD_TYPE_OBJECT},
 {40,"getRandomBytes",METHOD_TYPE_BOOLEAN},
 {10,"getPackageName",METHOD_TYPE_OBJECT},{11,"getVersionName",METHOD_TYPE_OBJECT},
 {12,"getLanguageCodeRAW",METHOD_TYPE_OBJECT},{13,"getCountryCodeRAW",METHOD_TYPE_OBJECT},
 {14,"getPreferredLangCodeRAW",METHOD_TYPE_OBJECT},{15,"nanotime",METHOD_TYPE_LONG},
 {31,"leaveBreadcrumb",METHOD_TYPE_VOID},{32,"sendLuaDump",METHOD_TYPE_VOID},
 {33,"sendException",METHOD_TYPE_VOID},{34,"beforeAssertFunction",METHOD_TYPE_VOID},
 {30,"stockAdditionalInfo",METHOD_TYPE_VOID},
 {20,"CreatePaint",METHOD_TYPE_OBJECT},{21,"getFontMetrics",METHOD_TYPE_OBJECT},
 {22,"setTextSize",METHOD_TYPE_VOID},{23,"setAntiAlias",METHOD_TYPE_VOID},
 {24,"measureText",METHOD_TYPE_FLOAT},{25,"drawText",METHOD_TYPE_VOID},
 {4,"getDeviceToken",METHOD_TYPE_OBJECT},
 {1,"getVersionSDK",METHOD_TYPE_INT},
 {2,"getOptimalAudioHz",METHOD_TYPE_INT},
 {3,"getOptimalAudioSamples",METHOD_TYPE_INT},
};

MethodsBoolean methodsBoolean[] = {{71,sif_jp_textbox_destroy},{72,sif_jp_textbox_setText},{80,sif_jp_webview_destroy},{81,sif_jp_webview_setText},{89,sif_jp_movieview_destroy},{90,sif_jp_movieview_setText},{94,sif_jp_indicator_destroy},{103,sif_jp_startMap},{109,sif_jp_getEnableNotification},{116,sif_jp_stopGetLocation},{119,sif_jp_eraseFolder},{61,sif_public_key_verify},{58,sif_keychain_set},{59,sif_keychain_delete},{40,sif_random_bytes}};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {{113,sif_jp_getFreeMemorySize},{114,sif_jp_getUsedMemorySize},{65,unix_time_now}};
MethodsFloat methodsFloat[] = {{124,sif_jp_getTheta},{125,sif_jp_getPhi},{129,sif_jp_MovieGetInfo},{24,sif_font_measure}};
MethodsInt methodsInt[] = {{70,sif_jp_textbox_create},{79,sif_jp_webview_create},{82,sif_jp_webview_getWebViewItem},{88,sif_jp_movieview_create},{93,sif_jp_indicator_create},{118,sif_jp_getPermissionStatus},{126,sif_jp_MovieCreate},{63,sif_aes_encrypt},{64,sif_aes_decrypt},{62,sif_public_key_encrypt},{55,android_debug_setting},{56,android_debug_setting},{1,version},{2,audio_hz},{3,audio_samples}};
MethodsLong methodsLong[] = {{15,nanotime}};
MethodsObject methodsObject[] = {{73,sif_jp_textbox_getText},{85,sif_jp_webview_getText},{86,sif_jp_webview_getTmpText},{96,sif_jp_generateDeviceIdent},{60,gen_user_id},{57,sif_keychain_get},{54,app_string},{53,sif_sha512},{52,sif_apk_signature_chars},{50,app_string},{51,app_string},{44,app_string},{45,app_string},{46,app_string},{47,app_string},{48,app_string},{49,app_string},{43,app_string},{42,sif_apk_signature},{41,date_time_now},{10,app_string},{11,app_string},{12,app_string},{13,app_string},{14,app_string},{4,device_token},{20,sif_paint_create},{21,sif_font_metrics}};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {{74,sif_jp_textbox_update},{75,sif_jp_textbox_setpaint},{76,sif_jp_textbox_sethint},{77,sif_jp_textbox_setMaxlen},{78,sif_jp_textbox_setAlignment},{83,sif_jp_webview_setZoom},{84,sif_jp_webview_setColor},{87,sif_jp_webview_update},{91,sif_jp_movieview_update},{92,sif_jp_movieview_cmd},{95,sif_jp_indicator_update},{97,sif_jp_billingConsume},{98,sif_jp_billingGetProducts},{99,sif_jp_billingBuyItem},{100,sif_jp_startMailer},{101,sif_jp_startAlertDialog},{102,sif_jp_startBrowser},{104,sif_jp_startSettings},{105,sif_jp_startApp},{106,sif_jp_clearCookies},{107,sif_jp_setLocalNotificationWithAlarm},{108,sif_jp_cancelLocalNotification},{110,sif_jp_exitGame},{111,sif_jp_quitGame},{112,sif_jp_copyToClipBoard},{115,sif_jp_getLocation},{117,sif_jp_requirePermission},{120,sif_jp_shareContents},{121,sif_jp_savePng2Album},{122,sif_jp_startObservation},{123,sif_jp_stopObservation},{127,sif_jp_MovieRelease},{128,sif_jp_MovieRefreshTexture},{130,sif_jp_MovieSetPlay},{131,sif_jp_MovieSetPause},{132,sif_jp_MovieResetLoop},{68,preload_rewarded_ad},{69,show_rewarded_ad},{67,extension_event},{66,forbid_sleep},{30,stock_info},{31,backtrace_message},{32,backtrace_message},{33,backtrace_message},{34,backtrace_message},{22,sif_font_size},{23,sif_font_antialias},{25,sif_draw_text}};

/*
 * JNI Fields
*/

// System-wide constant that applications sometimes request
// https://developer.android.com/reference/android/content/Context.html#WINDOW_SERVICE
char WINDOW_SERVICE[] = "window";

// System-wide constant that's often used to determine Android version
// https://developer.android.com/reference/android/os/Build.VERSION.html#SDK_INT
// Possible values: https://developer.android.com/reference/android/os/Build.VERSION_CODES
const int SDK_INT = SIF_ANDROID_SDK;

NameToFieldID nameToFieldId[] = {
        {100,"ascent",FIELD_TYPE_FLOAT},{101,"descent",FIELD_TYPE_FLOAT},
        {102,"top",FIELD_TYPE_FLOAT},{103,"bottom",FIELD_TYPE_FLOAT},{104,"leading",FIELD_TYPE_FLOAT},
		{ 2, "WINDOW_SERVICE", FIELD_TYPE_OBJECT },
        { 10, "m_path_install", FIELD_TYPE_OBJECT },
        { 11, "m_path_external", FIELD_TYPE_OBJECT }, 
		{ 1, "SDK_INT", FIELD_TYPE_INT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
		{ 1, SDK_INT },
};
FieldsObject fieldsObject[] = {
		{ 2, NULL },
        { 10, NULL },
        { 11, NULL },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES

/* Fail before game startup if a declared JNI binding lacks its typed handler.
 * Bind the complete reviewed signature set here, including A-form calls that
 * have not yet been encountered on the physical device. */
static void verify_platform_methods(void) {
    unsigned count=0;
#define JNI_PLATFORM(number,name,sig,type,ret,table) do { \
    unsigned found=0; \
    for(unsigned i=0;i<sizeof(methods##table)/sizeof(methods##table[0]);i++) \
        if(methods##table[i].id==number && methods##table[i].Method==sif_jp_##name)found++; \
    if(found!=1 || (uintptr_t)getMethodIdByName(#name)!=number)sif_stop("JNI platform table mismatch"); \
    sif_java_bind_method((jmethodID)number,sig);count++; \
} while(0);
#include "java_platform_methods.inc"
#undef JNI_PLATFORM
    for(unsigned i=0;i<sizeof(nameToMethodId)/sizeof(nameToMethodId[0]);i++)
        for(unsigned j=i+1;j<sizeof(nameToMethodId)/sizeof(nameToMethodId[0]);j++)
            if(nameToMethodId[i].id==nameToMethodId[j].id || !strcmp(nameToMethodId[i].name,nameToMethodId[j].name))
                sif_stop("Duplicate JNI method binding");
    sif_log("JNI platform audit: %u signatures and typed handlers verified",count);
}

void sif_java_init(void) { jni_init(); verify_platform_methods(); sif_java_platform_init(); ((struct JNINativeInterface*)jni)->GetFloatField=sif_font_field; fieldsObject[0].value=jni->NewStringUTF(&jni,WINDOW_SERVICE); fieldsObject[1].value=jni->NewStringUTF(&jni,INSTALL_PATH); fieldsObject[2].value=jni->NewStringUTF(&jni,EXTERNAL_PATH); }
