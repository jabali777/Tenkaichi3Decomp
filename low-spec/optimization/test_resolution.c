#include "bt3_reduced_resolution.c"
#include <assert.h>
static U bound_texture;
static char shader_source[512];
static void fake_shader(U shader,I count,const char *const *sources,const I *lengths) {
    (void)shader;assert(count==1);
    size_t n=lengths && lengths[0]>=0 ? (size_t)lengths[0] : strlen(sources[0]);
    assert(n<sizeof(shader_source));memcpy(shader_source,sources[0],n);shader_source[n]=0;
}
static I image_w,image_h,rect[4],blit[8],pack_align=1;
static void fake_get(U p,I *v) {
    *v=p==TEX_BINDING ? (I)bound_texture : p==PACK_ALIGN ? pack_align : 0;
}
static void fake_bind_texture(U target,U id) { assert(target==TEX2D); bound_texture=id; }
static void fake_image(U t,I l,I in,I w,I h,I b,U f,U type,const void *data) {
    (void)t;(void)l;(void)in;(void)b;(void)f;(void)type;(void)data; image_w=w; image_h=h;
}
static void fake_sub(U t,I l,I x,I y,I w,I h,U f,U ty,const void *data) {
    (void)t;(void)l;(void)x;(void)y;(void)w;(void)h;(void)f;(void)ty;(void)data;
}
static void fake_bind_fbo(U target,U id) { (void)target;(void)id; }
static void fake_attach(U t,U a,U tt,U id,I l) { (void)t;(void)a;(void)tt;(void)id;(void)l; }
static void fake_delete(I n,const U *ids) { (void)n;(void)ids; }
static void fake_rect(I x,I y,I w,I h) { rect[0]=x;rect[1]=y;rect[2]=w;rect[3]=h; }
static void fake_blit(I a,I b,I c,I d,I e,I f,I g,I h,U mask,U filter) {
    (void)mask;(void)filter; I values[]={a,b,c,d,e,f,g,h}; memcpy(blit,values,sizeof(blit));
}
static void fake_store(U p,I v) { assert(p==PACK_ALIGN); pack_align=v; }
static void fake_read(I x,I y,I w,I h,U format,U type,void *data) {
    assert(format==0x1908u && type==0x1401u);
    for(I j=0;j<h;j++) for(I i=0;i<w;i++) {
        unsigned char *p=(unsigned char *)data+(j*w+i)*4;
        p[0]=(unsigned char)(x+i);p[1]=(unsigned char)(y+j);p[2]=128;p[3]=255;
    }
}
int main(void) {
    real_glGetIntegerv=fake_get;real_glBindTexture=fake_bind_texture;
    real_glTexImage2D=fake_image;real_glTexSubImage2D=fake_sub;
    real_glBindFramebuffer=fake_bind_fbo;real_glFramebufferTexture2D=fake_attach;
    real_glDeleteTextures=fake_delete;real_glDeleteFramebuffers=fake_delete;
    real_glViewport=fake_rect;real_glScissor=fake_rect;real_glBlitFramebuffer=fake_blit;
    real_glPixelStorei=fake_store;real_glReadPixels=fake_read;
    real_glShaderSource=fake_shader;
    const char *gs="in vec3 vStq; void main(){ float s = abs(p.misc.w); }";
    wrap_glShaderSource(1,1,&gs,NULL);
    assert(strstr(shader_source,"abs(p.misc.w) * 0.5;"));
    const char *hud="void main(){ float s = abs(p.misc.w); }";
    wrap_glShaderSource(1,1,&hud,NULL);
    assert(!strcmp(shader_source,hud)); /* Unrelated HUD shaders keep their coordinates. */
    I source_length=12;
    wrap_glShaderSource(1,1,&gs,&source_length);
    assert(strlen(shader_source)==12); /* Explicit source lengths are honored. */
    bound_texture=10;
    wrap_glTexImage2D(TEX2D,0,0x8058,1024,1024,0,0x1908u,0x1401u,NULL);
    wrap_glBindFramebuffer(FBO,3);
    wrap_glFramebufferTexture2D(FBO,0x8CE0u,TEX2D,10,0);
    assert(textures[10].small && image_w==512 && image_h==512 && factor(3)==2);
    wrap_glViewport(0,0,1024,1024);assert(rect[2]==512 && rect[3]==512);
    wrap_glScissor(3,5,4,6);assert(rect[0]==1 && rect[1]==2 && rect[2]==3 && rect[3]==4);
    unsigned char pixels[4*4*4]={0};
    wrap_glReadPixels(0,0,4,4,0x1908u,0x1401u,pixels);
    assert(pixels[0]==0 && pixels[4]==0 && pixels[8]==1 && pixels[16*2+1]==1);
    wrap_glBindFramebuffer(DRAW_FBO,0);
    wrap_glBlitFramebuffer(0,0,512,448,0,0,640,480,1,1);
    assert(blit[2]==256 && blit[3]==224 && blit[6]==640 && blit[7]==480);
    wrap_glTexSubImage2D(TEX2D,0,0,0,1024,1024,0x1908u,0x1401u,pixels);
    assert(!textures[10].small && image_w==1024 && factor(3)==1);
    bound_texture=11;
    wrap_glTexImage2D(TEX2D,0,0x8058,1024,1024,0,0x1908u,0x1401u,pixels);
    wrap_glBindFramebuffer(FBO,4);
    wrap_glFramebufferTexture2D(FBO,0x8CE0u,TEX2D,11,0);
    assert(!textures[11].small && factor(4)==1); /* Asset images are not reduced. */
    U fbo=4,tex=11; wrap_glDeleteFramebuffers(1,&fbo);wrap_glDeleteTextures(1,&tex);
    assert(draw_fbo==0 && read_fbo==0 && !textures[11].eligible);
    assert(down(-3)==-2 && up(-3)==-1 && up(3)==2);
    puts("half resolution: all checks passed");
    return 0;
}
