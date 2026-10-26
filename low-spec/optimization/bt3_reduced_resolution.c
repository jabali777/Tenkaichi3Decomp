/* Optional half-resolution renderer for this port's 1024x1024 GL targets.
 * Asset textures, default framebuffer, simulation and input remain untouched.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int U;
typedef int I;
typedef void (*Proc)(void);
#define CAP 65536u
#define TEX2D 0x0DE1u
#define TEX_BINDING 0x8069u
#define FBO 0x8D40u
#define READ_FBO 0x8CA8u
#define DRAW_FBO 0x8CA9u
#define PACK_ALIGN 0x0D05u

typedef struct { I w,h,internal; U format,type; int eligible,small; } Texture;
typedef struct { U attachments[3]; } Framebuffer;
static Texture textures[CAP];
static Framebuffer fbos[CAP];
static U read_fbo,draw_fbo;
static int announced;
static Proc (*next_getproc)(const char *);
#define DECL(n,r,a) static r (*real_##n) a
DECL(glBindFramebuffer,void,(U,U));
DECL(glBindTexture,void,(U,U));
DECL(glTexImage2D,void,(U,I,I,I,I,I,U,U,const void *));
DECL(glTexSubImage2D,void,(U,I,I,I,I,I,U,U,const void *));
DECL(glFramebufferTexture2D,void,(U,U,U,U,I));
DECL(glDeleteTextures,void,(I,const U *));
DECL(glDeleteFramebuffers,void,(I,const U *));
DECL(glViewport,void,(I,I,I,I));
DECL(glScissor,void,(I,I,I,I));
DECL(glBlitFramebuffer,void,(I,I,I,I,I,I,I,I,U,U));
DECL(glReadPixels,void,(I,I,I,I,U,U,void *));
DECL(glGetIntegerv,void,(U,I *));
DECL(glPixelStorei,void,(U,I));
DECL(glShaderSource,void,(U,I,const char *const *,const I *));
#undef DECL

static void init(void) {
    if(next_getproc) return;
    *(void **)(&next_getproc)=dlsym(RTLD_NEXT,"SDL_GL_GetProcAddress");
    if(!next_getproc) abort();
#define LOAD(n) real_##n=(void *)next_getproc(#n)
    LOAD(glBindFramebuffer); LOAD(glBindTexture); LOAD(glTexImage2D);
    LOAD(glTexSubImage2D); LOAD(glFramebufferTexture2D); LOAD(glDeleteTextures);
    LOAD(glDeleteFramebuffers); LOAD(glViewport); LOAD(glScissor);
    LOAD(glBlitFramebuffer); LOAD(glReadPixels); LOAD(glGetIntegerv);
    LOAD(glPixelStorei); LOAD(glShaderSource);
#undef LOAD
}
static int factor(U fbo) {
    if(!fbo || fbo>=CAP) return 1;
    for(int j=0;j<3;j++) {
        U t=fbos[fbo].attachments[j];
        if(t && t<CAP) return textures[t].small ? 2 : 1;
    }
    return 1;
}
static I down(I n) { return n>=0 ? n/2 : (I)(-((-(long long)n+1)/2)); }
static I up(long long n) { return n>=0 ? (I)((n+1)/2) : (I)(-(-n/2)); }
static void wrap_glBindFramebuffer(U target,U id) {
    if(target==FBO || target==READ_FBO) read_fbo=id;
    if(target==FBO || target==DRAW_FBO) draw_fbo=id;
    real_glBindFramebuffer(target,id);
}
static void wrap_glTexImage2D(U target,I level,I internal,I w,I h,I border,U format,U type,const void *data) {
    if(target==TEX2D && level==0) {
        I id=0; real_glGetIntegerv(TEX_BINDING,&id);
        if(id>0 && (U)id<CAP) textures[id]=(Texture){w,h,internal,format,type,!data && !border && w==1024 && h==1024,0};
    }
    real_glTexImage2D(target,level,internal,w,h,border,format,type,data);
}
static void wrap_glFramebufferTexture2D(U target,U attachment,U textarget,U texture,I level) {
    U id=target==READ_FBO ? read_fbo : draw_fbo;
    int slot=attachment==0x8CE0u ? 0 : attachment==0x8CE1u ? 1 :
             (attachment==0x821Au || attachment==0x8D00u) ? 2 : -1;
    if(id && id<CAP && slot>=0 && textarget==TEX2D && level==0) {
        fbos[id].attachments[slot]=texture;
        if(texture && texture<CAP) {
            Texture *t=&textures[texture];
            if(t->eligible && !t->small) {
                I old=0; real_glGetIntegerv(TEX_BINDING,&old);
                real_glBindTexture(TEX2D,texture);
                real_glTexImage2D(TEX2D,0,t->internal,t->w/2,t->h/2,0,t->format,t->type,NULL);
                real_glBindTexture(TEX2D,(U)old); t->small=1;
                if(!announced++) fputs("bt3-lowres: effective internal picture 256x224\n",stderr);
            }
        }
    }
    real_glFramebufferTexture2D(target,attachment,textarget,texture,level);
}
static void wrap_glTexSubImage2D(U target,I level,I x,I y,I w,I h,U format,U type,const void *data) {
    if(target==TEX2D && level==0) {
        I id=0; real_glGetIntegerv(TEX_BINDING,&id);
        if(id>0 && (U)id<CAP) {
            Texture *t=&textures[id];
            /* Movie upload textures are full-sized sources, not render destinations. */
            if(t->small && ((long long)x+w>t->w/2 || (long long)y+h>t->h/2)) {
                real_glTexImage2D(TEX2D,0,t->internal,t->w,t->h,0,t->format,t->type,NULL);
                t->small=0; t->eligible=0;
            }
        }
    }
    real_glTexSubImage2D(target,level,x,y,w,h,format,type,data);
}
static void wrap_glDeleteTextures(I n,const U *ids) {
    for(I j=0;j<n;j++) if(ids[j]<CAP) memset(&textures[ids[j]],0,sizeof(Texture));
    real_glDeleteTextures(n,ids);
}
static void wrap_glDeleteFramebuffers(I n,const U *ids) {
    for(I j=0;j<n;j++) {
        if(ids[j]<CAP) memset(&fbos[ids[j]],0,sizeof(Framebuffer));
        if(read_fbo==ids[j]) read_fbo=0;
        if(draw_fbo==ids[j]) draw_fbo=0;
    }
    real_glDeleteFramebuffers(n,ids);
}
static void wrap_glViewport(I x,I y,I w,I h) {
    if(factor(draw_fbo)==2 && w>=0 && h>=0) real_glViewport(down(x),down(y),w/2,h/2);
    else real_glViewport(x,y,w,h);
}
static void wrap_glScissor(I x,I y,I w,I h) {
    if(factor(draw_fbo)==2 && w>=0 && h>=0) {
        I a=down(x),b=down(y);
        real_glScissor(a,b,up((long long)x+w)-a,up((long long)y+h)-b);
    } else real_glScissor(x,y,w,h);
}
static void wrap_glBlitFramebuffer(I a,I b,I c,I d,I e,I f,I g,I h,U mask,U filter) {
    int s=factor(read_fbo),t=factor(draw_fbo);
    real_glBlitFramebuffer(a/s,b/s,c/s,d/s,e/t,f/t,g/t,h/t,mask,filter);
}
static void wrap_glReadPixels(I x,I y,I w,I h,U format,U type,void *data) {
    int channels=format==0x1908u ? 4 : format==0x1907u ? 3 : format==0x1903u ? 1 : 0;
    I pbo=0,row_length=0;
    real_glGetIntegerv(0x88EDu,&pbo);
    real_glGetIntegerv(0x0D02u,&row_length);
    if(factor(read_fbo)!=2 || !data || !channels || type!=0x1401u || pbo || row_length || w<=0 || h<=0 || w>8192 || h>8192) {
        real_glReadPixels(x,y,w,h,format,type,data); return;
    }
    I sx=down(x),sy=down(y),sw=up((long long)x+w)-sx,sh=up((long long)y+h)-sy;
    unsigned char *pixels=malloc((size_t)sw*(size_t)sh*(size_t)channels);
    if(!pixels) abort();
    I alignment=4; real_glGetIntegerv(PACK_ALIGN,&alignment);
    real_glPixelStorei(PACK_ALIGN,1);
    real_glReadPixels(sx,sy,sw,sh,format,type,pixels);
    real_glPixelStorei(PACK_ALIGN,alignment);
    size_t stride=((size_t)w*(size_t)channels+(size_t)alignment-1)/(size_t)alignment*(size_t)alignment;
    for(I j=0;j<h;j++) for(I i=0;i<w;i++) {
        I ix=down(x+i)-sx,iy=down(y+j)-sy;
        memcpy((unsigned char *)data+(size_t)j*stride+(size_t)i*channels,
               pixels+((size_t)iy*sw+(size_t)ix)*channels,(size_t)channels);
    }
    free(pixels);
}
static void wrap_glShaderSource(U shader,I count,const char *const *strings,const I *lengths) {
    const char *before="float s = abs(p.misc.w);";
    const char *after="float s = abs(p.misc.w) * 0.5;";
    /* This port supplies one source string. Keep other loaders unchanged. */
    if(count==1 && strings && strings[0]) {
        size_t n=lengths && lengths[0]>=0 ? (size_t)lengths[0] : strlen(strings[0]);
        if(n<=1048576u) {
            char *source=malloc(n+strlen(after)+1);
            if(source) {
                memcpy(source,strings[0],n); source[n]=0;
                char *at=strstr(source,before);
                if(at && strstr(source,"vStq")) {
                    size_t tail=strlen(at+strlen(before));
                    memmove(at+strlen(after),at+strlen(before),tail+1);
                    memcpy(at,after,strlen(after));
                    const char *compiled=source;
                    real_glShaderSource(shader,1,&compiled,NULL);
                    free(source); return;
                }
                free(source);
            }
        }
    }
    real_glShaderSource(shader,count,strings,lengths);
}
Proc SDL_GL_GetProcAddress(const char *name) {
    init();
#define HOOK(n) if(!strcmp(name,#n)) return (Proc)wrap_##n
    HOOK(glBindFramebuffer); HOOK(glTexImage2D); HOOK(glTexSubImage2D);
    HOOK(glFramebufferTexture2D); HOOK(glDeleteTextures); HOOK(glDeleteFramebuffers);
    HOOK(glViewport); HOOK(glScissor); HOOK(glBlitFramebuffer); HOOK(glReadPixels);
    HOOK(glShaderSource);
#undef HOOK
    return next_getproc(name);
}
