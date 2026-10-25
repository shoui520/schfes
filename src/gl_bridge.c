#include <vitaGL.h>
#include "boundary.h"
#include "sif.h"
#include <openssl/sha.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void trace_vertex_inputs(GLenum type,GLsizei count,const void *indices) {
    GLint element_buffer=0,texture=0;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&element_buffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
    if(element_buffer || type!=GL_UNSIGNED_SHORT || count<1 || !indices)return;
    unsigned vertex=((const uint16_t*)indices)[0];
    if(vertex>4096)return;
    sif_log("GL input texture=%d first-index=%u",texture,vertex);
    if(count>=6) {
        const uint16_t *ix=indices;
        sif_log("GL input indices=%u,%u,%u,%u,%u,%u",ix[0],ix[1],ix[2],ix[3],ix[4],ix[5]);
    }
    for(unsigned i=0;i<3;i++) {
        GLint enabled=0,buffer=0,size=0,stride=0,format=0;void *pointer=NULL;
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&enabled);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&buffer);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&size);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&stride);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&format);
        glGetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);
        sif_log("GL input attribute=%u enabled=%d buffer=%d size=%d stride=%d type=%x pointer=%p",
            i,enabled,buffer,size,stride,format,pointer);
        if(!enabled || buffer || !pointer || size<1 || size>4 || stride<0 || stride>256)continue;
        unsigned bytes=format==GL_FLOAT?4:format==GL_UNSIGNED_BYTE?1:0;
        if(!bytes)continue;
        const unsigned char *p=(const unsigned char*)pointer+vertex*(stride?stride:size*bytes);
        if(format==GL_FLOAT) {
            float values[4]={0};memcpy(values,p,size*bytes);
            sif_log("GL input values[%u]=%.4f,%.4f,%.4f,%.4f",i,values[0],values[1],values[2],values[3]);
            if(i==0 && count>=6 && size==2)for(unsigned k=1;k<3;k++) {
                unsigned ix=((const uint16_t*)indices)[k];
                if(ix>4096)continue;
                memcpy(values,(const unsigned char*)pointer+ix*(stride?stride:size*bytes),size*bytes);
                sif_log("GL triangle vertex %u index=%u xy=%.4f,%.4f",k,ix,values[0],values[1]);
            }
        } else {
            unsigned char values[4]={0};memcpy(values,p,size);
            sif_log("GL input bytes[%u]=%u,%u,%u,%u",i,values[0],values[1],values[2],values[3]);
        }
    }
}
void sif_glPixelStorei(GLenum name,GLint value) {
    if(name==GL_PACK_ALIGNMENT || name==GL_UNPACK_ALIGNMENT) {
        // This VitaGL backend always transfers tightly packed rows. SIF's
        // explicit alignment=1 therefore needs no backend state change.
        if(value!=1) {
            sif_log("Unsupported GL pixel alignment name=%x value=%d",name,value);
            sif_stop("Pixel row padding needs conversion");
        }
        static unsigned calls;
        if(calls++<4)sif_log("GL pixel alignment name=%x value=1 (tight rows)",name);
        return;
    }
    glPixelStorei(name,value);
}
GLenum sif_glGetError(void) {
    GLenum error=glGetError();
    if(error)sif_log("GL error=%x caller=%p",error,__builtin_return_address(0));
    return error;
}
void sif_glTexSubImage2D(GLenum target,GLint level,GLint x,GLint y,GLsizei width,GLsizei height,GLenum format,GLenum type,const void *pixels) {
    static unsigned calls;
    if(calls++<64)sif_log("GL texture update target=%x level=%d xy=%d,%d size=%dx%d format=%x type=%x pixels=%p",
        target,level,x,y,width,height,format,type,pixels);
    glTexSubImage2D(target,level,x,y,width,height,format,type,pixels);
}
void sif_glTexImage2D(GLenum target,GLint level,GLint internal,GLsizei width,GLsizei height,GLint border,GLenum format,GLenum type,const void *pixels) {
    static unsigned calls;
    if(calls++<64)sif_log("GL texture image target=%x level=%d internal=%x size=%dx%d border=%d format=%x type=%x pixels=%p",
        target,level,internal,width,height,border,format,type,pixels);
    glTexImage2D(target,level,internal,width,height,border,format,type,pixels);
}
/* The engine clears to pure green and presents those empty frames while its
 * first textures load, which on Vita takes long enough to be seen. Until the
 * first draw, clear to black; the requested color applies from then on. */
static int drawn;
static float boot_clear[4];
SOFT_ENTRY void sif_glClearColor(float r,float g,float b,float a) {
    static unsigned calls;
    if(calls++<8)sif_log("GL clear color %.3f %.3f %.3f %.3f",r,g,b,a);
    if(!drawn){boot_clear[0]=r;boot_clear[1]=g;boot_clear[2]=b;boot_clear[3]=a;glClearColor(0,0,0,1);return;}
    glClearColor(r,g,b,a);
}
void sif_glDrawElements(GLenum mode,GLsizei count,GLenum type,const void *indices) {
    static unsigned calls;
    if(!drawn){drawn=1;glClearColor(boot_clear[0],boot_clear[1],boot_clear[2],boot_clear[3]);}
    if(calls<3 || calls==300)trace_vertex_inputs(type,count,indices);
    if(calls++<12) {
        GLint program=0,fb=0,viewport[4]={0},scissor[4]={0},array=0,elements=0;
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fb);
        glGetIntegerv(GL_VIEWPORT,viewport);
        glGetIntegerv(GL_SCISSOR_BOX,scissor);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&elements);
        sif_log("GL draw mode=%x count=%d type=%x program=%d fb=%d viewport=%d,%d,%d,%d",
            mode,count,type,program,fb,viewport[0],viewport[1],viewport[2],viewport[3]);
        sif_log("GL draw buffers=%d,%d scissor=%d:%d,%d,%d,%d depth=%d cull=%d blend=%d",
            array,elements,glIsEnabled(GL_SCISSOR_TEST),scissor[0],scissor[1],scissor[2],scissor[3],
            glIsEnabled(GL_DEPTH_TEST),glIsEnabled(GL_CULL_FACE),glIsEnabled(GL_BLEND));
    }
    glDrawElements(mode,count,type,indices);
}
void sif_glBindAttribLocation(GLuint program,GLuint index,const GLchar *name) {
    sif_log("GL attribute program=%u index=%u name=%s",program,index,name);
    glBindAttribLocation(program,index,name);
}
void sif_glVertexAttribPointer(GLuint index,GLint size,GLenum type,GLboolean normalized,GLsizei stride,const void *pointer) {
    static unsigned calls;
    if(calls++<12)sif_log("GL attribute pointer index=%u size=%d type=%x normalized=%d stride=%d pointer=%p",
        index,size,type,normalized,stride,pointer);
    glVertexAttribPointer(index,size,type,normalized,stride,pointer);
}
void sif_glUniformMatrix4fv(GLint location,GLsizei count,GLboolean transpose,const GLfloat *value) {
    static unsigned calls;
    if(calls++<4 && count>0 && value) {
        sif_log("GL matrix location=%d count=%d transpose=%d",location,count,transpose);
        for(unsigned i=0;i<4;i++)sif_log("GL matrix column %u: %.5f %.5f %.5f %.5f",i,
            value[4*i],value[4*i+1],value[4*i+2],value[4*i+3]);
    }
    glUniformMatrix4fv(location,count,transpose,value);
}
void sif_glBindFramebuffer(GLenum target,GLuint framebuffer) {
    static unsigned calls;
    if(calls++<12)sif_log("GL framebuffer target=%x name=%u",target,framebuffer);
    glBindFramebuffer(target,framebuffer);
}
SOFT_ENTRY void sif_glClearDepthf(float d) { glClearDepthf(d); }
SOFT_ENTRY void sif_glDepthRangef(float n,float f) { glDepthRangef(n,f); }
void glShaderSource_soloader(GLuint shader,GLsizei count,const GLchar **src,const GLint *len) {
    if(count>0 && src) {
        SHA256_CTX ctx;SHA256_Init(&ctx);unsigned char hash[32];char hex[65],path[128];
        sceIoMkdir(DATA_PATH "agent",0777);sceIoMkdir(DATA_PATH "agent/shaders",0777);
        snprintf(path,sizeof(path),DATA_PATH "agent/shaders/%u.glsl",shader);FILE *f=fopen(path,"wb");
        size_t total=0;int saved=f!=NULL;
        for(int i=0;i<count;i++)if(src[i]) {
            size_t n=len && len[i]>=0?(size_t)len[i]:strlen(src[i]);
            SHA256_Update(&ctx,src[i],n);total+=n;
            if(f && fwrite(src[i],1,n,f)!=n)saved=0;
        }
        if(f && fclose(f))saved=0;SHA256_Final(hash,&ctx);
        for(unsigned i=0;i<32;i++)sprintf(hex+i*2,"%02x",hash[i]);
        sif_log("Game shader %u source bytes=%u SHA256=%s saved=%d",shader,(unsigned)total,hex,saved);
    }
    glShaderSource(shader,count,src,len);
}
void sif_glLinkProgram(GLuint program) {
    glLinkProgram(program);GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok) {char b[2048];glGetProgramInfoLog(program,sizeof(b),NULL,b);sif_log("Program %u: %s",program,b);sif_stop("shader link failed");}
    sif_log("Game shader program %u linked",program);
    GLint length=0;glGetProgramiv(program,GL_PROGRAM_BINARY_LENGTH,&length);
    if(length>0 && length<1024*1024) {
        void *binary=malloc(length);
        if(binary) {
            GLsizei written=0;GLenum format=0;char path[128];
            glGetProgramBinary(program,length,&written,&format,binary);
            snprintf(path,sizeof(path),DATA_PATH "agent/shaders/program-%u.bin",program);
            FILE *file=fopen(path,"wb");
            if(file){fwrite(binary,1,written,file);fclose(file);}
            sif_log("GL program binary format=%x bytes=%d",format,written);
            free(binary);
        }
    }
}
void glCompileShader_soloader(GLuint shader) {
    glCompileShader(shader); GLint ok; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) { char b[2048]; glGetShaderInfoLog(shader,sizeof(b),NULL,b); sif_log("Shader %u: %s",shader,b); sif_stop("shader compilation failed"); }
}
