#include "fonts.h"
#include "sif.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static FT_Library ft;
struct Face {char name[256];FT_Face face;};
static struct Face faces[8];
struct Metrics {uint32_t magic;float ascent,descent,top,bottom,leading;};
struct Paint {uint32_t magic;FT_Face face;float size;int antialias,bold;struct Metrics metrics;};
#define PAINT_MAGIC 0x5041494e
#define METRICS_MAGIC 0x464d4554
static struct Paint *paint_arg(va_list a) {
    struct Paint *p=va_arg(a,struct Paint*);
    if(!p || p->magic!=PAINT_MAGIC)sif_stop("Invalid Paint object");return p;
}
static void select_size(struct Paint *p) {
    if(FT_Set_Char_Size(p->face,0,(FT_F26Dot6)(p->size*64),72,72))sif_stop("Font size failed");
}
jobject sif_paint_create(jmethodID id,va_list a) {
    jstring name=va_arg(a,jstring);int bold=va_arg(a,int);
    const char *text=name?jni->GetStringUTFChars(&jni,name,NULL):"MTLmr3m.ttf";
    if(!text || strchr(text,':') || text[0]=='/' || strstr(text,".."))sif_stop("Invalid font asset name");
    if(!ft && FT_Init_FreeType(&ft))sif_stop("FreeType init failed");
    FT_Face face=NULL;int empty=-1;
    for(int i=0;i<8;i++) {if(faces[i].face && !strcmp(faces[i].name,text))face=faces[i].face;if(!faces[i].face && empty<0)empty=i;}
    if(!face) {
        if(empty<0)sif_stop("Font face cache full");char path[1024];snprintf(path,sizeof(path),"%s%s",INSTALL_PATH,text);
        if(FT_New_Face(ft,path,0,&face)) {sif_log("Font load failed: %s",text);sif_stop("Font asset unavailable");}
        strncpy(faces[empty].name,text,sizeof(faces[empty].name)-1);faces[empty].face=face;
        sif_log("Font loaded %s glyphs=%ld",text,face->num_glyphs);
    }
    if(name)jni->ReleaseStringUTFChars(&jni,name,(char*)text);
    struct Paint *p=calloc(1,sizeof(*p));if(!p)sif_stop("Paint allocation failed");
    p->magic=PAINT_MAGIC;p->face=face;p->size=12;p->antialias=1;p->bold=!name && bold;return p;
}
void sif_font_size(jmethodID id,va_list a) { struct Paint *p=paint_arg(a);double size=va_arg(a,double);if(size<=0 || size>2048)sif_stop("Invalid font size");p->size=size*5.0f/6.0f;select_size(p); }
void sif_font_antialias(jmethodID id,va_list a) {struct Paint *p=paint_arg(a);p->antialias=va_arg(a,int);}
jobject sif_font_metrics(jmethodID id,va_list a) {
    struct Paint *p=paint_arg(a);select_size(p);FT_Face f=p->face;
    p->metrics=(struct Metrics){METRICS_MAGIC,-f->size->metrics.ascender/64.0f,-f->size->metrics.descender/64.0f,
        -FT_MulFix(f->bbox.yMax,f->size->metrics.y_scale)/64.0f,-FT_MulFix(f->bbox.yMin,f->size->metrics.y_scale)/64.0f,
        (f->size->metrics.height-f->size->metrics.ascender+f->size->metrics.descender)/64.0f};
    return &p->metrics;
}
jfloat sif_font_field(JNIEnv *env,jobject obj,jfieldID id) {
    struct Metrics *m=obj;unsigned field=(uintptr_t)id;
    if(!m || m->magic!=METRICS_MAGIC || field<100 || field>104)sif_stop("Invalid font metrics field");
    return (&m->ascent)[field-100];
}
static uint32_t next_char(const unsigned char **text) {
    const unsigned char *p=*text;unsigned c=*p++;
    if(c>=0xf0){c=(c&7)<<18;c|=(*p++&63)<<12;c|=(*p++&63)<<6;c|=*p++&63;}
    else if(c>=0xe0){c=(c&15)<<12;c|=(*p++&63)<<6;c|=*p++&63;}
    else if(c>=0xc0){c=(c&31)<<6;c|=*p++&63;}
    *text=p;return c;
}
static float layout(struct Paint *p,const char *text,uint32_t *dst,unsigned width,unsigned height,uint32_t color,float x,float y) {
    select_size(p);FT_Face f=p->face;FT_UInt previous=0;float start=x;
    const unsigned char *s=(const unsigned char*)text;
    while(*s) {
        uint32_t cp=next_char(&s);FT_UInt index=FT_Get_Char_Index(f,cp);
        if(previous && index && FT_HAS_KERNING(f)){FT_Vector k;FT_Get_Kerning(f,previous,index,FT_KERNING_DEFAULT,&k);x+=k.x/64.0f;}
        if(FT_Load_Glyph(f,index,FT_LOAD_DEFAULT))sif_stop("Glyph load failed");
        if(p->bold)FT_GlyphSlot_Embolden(f->glyph);
        if(dst) {
            if(FT_Render_Glyph(f->glyph,p->antialias?FT_RENDER_MODE_NORMAL:FT_RENDER_MODE_MONO))sif_stop("Glyph render failed");
            FT_Bitmap *b=&f->glyph->bitmap;int left=(int)x+f->glyph->bitmap_left,top=(int)y-f->glyph->bitmap_top;
            for(unsigned row=0;row<b->rows;row++)for(unsigned col=0;col<b->width;col++) {
                int px=left+col,py=top+row;if(px<0 || py<0 || px>=(int)width || py>=(int)height)continue;
                unsigned char *line=b->pitch>=0?b->buffer+row*b->pitch:b->buffer+(b->rows-row-1)*(-b->pitch);
                unsigned cov=b->pixel_mode==FT_PIXEL_MODE_MONO?((line[col/8]&(128>>(col%8)))?255:0):line[col];
                unsigned sa=cov*(color>>24)/255;if(!sa)continue;
                uint32_t old=dst[py*width+px];unsigned da=old>>24,oa=sa+da*(255-sa)/255;uint32_t value=oa<<24;
                for(unsigned shift=0;shift<24;shift+=8) {unsigned src=(color>>shift)&255,d=(old>>shift)&255;value|=((src*sa+d*da*(255-sa)/255)/oa)<<shift;}
                dst[py*width+px]=value;
            }
        }
        x+=f->glyph->advance.x/64.0f;previous=index;
    }
    return x-start;
}
jfloat sif_font_measure(jmethodID id,va_list a) {
    struct Paint *p=paint_arg(a);jstring js=va_arg(a,jstring);const char *s=jni->GetStringUTFChars(&jni,js,NULL);
    float result=layout(p,s,NULL,0,0,0,0,0);jni->ReleaseStringUTFChars(&jni,js,(char*)s);return result;
}
void sif_draw_text(jmethodID id,va_list a) {
    struct Paint *p=paint_arg(a);jintArray image=va_arg(a,jintArray);unsigned color=va_arg(a,unsigned);
    int w=va_arg(a,int),h=va_arg(a,int);double x=va_arg(a,double),y=va_arg(a,double);jstring js=va_arg(a,jstring);
    if(w<=0 || h<=0 || w>4096 || h>4096 || jni->GetArrayLength(&jni,image)!=w*h)sif_stop("Invalid text bitmap");
    jint *pixels=jni->GetIntArrayElements(&jni,image,NULL);const char *s=jni->GetStringUTFChars(&jni,js,NULL);
    p->antialias=1;
    layout(p,s,(uint32_t*)pixels,w,h,color,x,y);
    jni->ReleaseStringUTFChars(&jni,js,(char*)s);jni->ReleaseIntArrayElements(&jni,image,pixels,0);
}
/* Rasterize Vita text controls with the same bundled face as game Paint. */
void sif_font_widget(jobject object,const char *text,unsigned *pixels,int w,int h,int alignment,int enabled) {
    static struct Paint fallback;
    struct Paint *p=object;
    if(!p){
        if(!fallback.magic){
            if(!ft && FT_Init_FreeType(&ft))sif_stop("Widget font init failed");
            FT_Face face;
            char path[1024];snprintf(path,sizeof(path),"%sMTLmr3m.ttf",INSTALL_PATH);
            if(FT_New_Face(ft,path,0,&face))sif_stop("Widget font unavailable");
            fallback=(struct Paint){.magic=PAINT_MAGIC,.face=face,.size=20,.antialias=1};
        }
        p=&fallback;
    }
    if(p->magic!=PAINT_MAGIC)sif_stop("Invalid textbox Paint");
    float length=layout(p,text,NULL,0,0,0,0,0);
    float x=10;if(alignment==1)x=(w-length)/2;else if(alignment==2)x=w-length-10;if(x<10)x=10;
    select_size(p);float baseline=(h+(p->face->size->metrics.ascender+p->face->size->metrics.descender)/64.0f)/2;
    layout(p,text,pixels,w,h,enabled?0xff202020:0xff808080,x,baseline);
}
