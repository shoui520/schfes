/* Hard-float VitaGL calls stay in this translation unit. Public entrypoints
 * take only integers/pointers, so they share the softfp argument convention. */
#include <vitaGL.h>
#include "java_platform.h"
static GLint program,matrix,active,texture,framebuffer,texenv;
static GLfloat current_color[4];
static GLboolean other_textures[8];
static GLint texture_units;
static GLboolean textured;
void sif_overlay_begin(void) {
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_MATRIX_MODE,&matrix);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);textured=glIsEnabled(GL_TEXTURE_2D);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&framebuffer);
    glGetFloatv(GL_CURRENT_COLOR,current_color);glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&texenv);
    glGetIntegerv(GL_MAX_TEXTURE_UNITS,&texture_units);if(texture_units>8)texture_units=8;
    for(int i=1;i<texture_units;i++){glActiveTexture(GL_TEXTURE0+i);other_textures[i]=glIsEnabled(GL_TEXTURE_2D);glDisable(GL_TEXTURE_2D);}
    glActiveTexture(GL_TEXTURE0);
    glPushAttrib(GL_ALL_ATTRIB_BITS);glUseProgram(0);glBindFramebuffer(GL_FRAMEBUFFER,0);
    glViewport(0,0,960,544);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);glDisable(GL_LIGHTING);glDisable(GL_FOG);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColorMask(1,1,1,1);
    glEnable(GL_TEXTURE_2D);glColor4f(1,1,1,1);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,960,544,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
}
void sif_overlay_bitmap(unsigned *handle,const unsigned *pixels,int w,int h,int x,int y) {
    if(!*handle)glGenTextures(1,handle);glBindTexture(GL_TEXTURE_2D,*handle);
    if(pixels){glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);}
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(x,y);glTexCoord2f(1,0);glVertex2f(x+w,y);glTexCoord2f(1,1);glVertex2f(x+w,y+h);glTexCoord2f(0,1);glVertex2f(x,y+h);glEnd();
}
void sif_overlay_end(void) {
    glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(matrix);
    glPopAttrib();glBindTexture(GL_TEXTURE_2D,texture);if(!textured)glDisable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,texenv);glColor4fv(current_color);
    for(int i=1;i<texture_units;i++){glActiveTexture(GL_TEXTURE0+i);if(other_textures[i])glEnable(GL_TEXTURE_2D);}
    glActiveTexture(active);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glUseProgram(program);
}
void sif_overlay_delete(unsigned texture){glDeleteTextures(1,&texture);}
