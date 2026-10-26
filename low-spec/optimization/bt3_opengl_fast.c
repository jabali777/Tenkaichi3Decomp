/* Local SDL3/OpenGL adapter for Tenkaichi3Decomp, Linux x86-64.
 * Streams small uniform updates through aligned ranges, orphaning storage on
 * wrap; identical updates are omitted. Optional simpler fighter shading changes visuals only; game simulation and assets are unchanged.
 * Build with build.sh. Disable in the launcher with BT3_OPTIMIZACION_GL=0.
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
#define UBO 0x8A11u
#define UBO_ALIGN 0x8A34u
#define STREAM_DRAW 0x88E0u
#define CAP 65536u
#define BINDINGS 64u
#define RING_BYTES 1048576u

typedef struct {
    unsigned char *shadow;
    size_t logical, capacity, stride, next, current;
    int valid, dirty;
} UniformBuffer;
static UniformBuffer uniforms[CAP];
static U indexed_uniforms[BINDINGS], bound_uniform;
static int announced;
static Proc (*next_getproc)(const char *);
static void (*real_glBindBuffer)(U,U);
static void (*real_glBindBufferBase)(U,U,U);
static void (*real_glBindBufferRange)(U,U,U,ptrdiff_t,ptrdiff_t);
static void (*real_glBufferData)(U,ptrdiff_t,const void *,U);
static void (*real_glBufferSubData)(U,ptrdiff_t,ptrdiff_t,const void *);
static void (*real_glDeleteBuffers)(I,const U *);
static void (*real_glGetIntegerv)(U,I *);

static int simple_shading;
static U current_program;
typedef struct { int fighter, checked; I layer_offset, binding, screen_offset; } Program;
static unsigned char fighter_shaders[CAP];
static Program programs[CAP];
static void (*real_glShaderSource)(U,I,const char *const *,const I *);
static void (*real_glAttachShader)(U,U);
static void (*real_glLinkProgram)(U);
static void (*real_glUseProgram)(U);
static void (*real_glDeleteProgram)(U);
static void (*real_glDeleteShader)(U);
static void (*real_glGetUniformIndices)(U,I,const char *const *,U *);
static void (*real_glGetActiveUniformsiv)(U,I,const U *,U,I *);
static void (*real_glGetActiveUniformBlockiv)(U,U,U,I *);
static void (*real_glDrawElements)(U,I,U,const void *);
static void (*real_glDrawArrays)(U,I,I);
static void (*real_glUniformBlockBinding)(U,U,U);

static void init(void) {
    if (next_getproc) return;
    *(void **)(&next_getproc)=dlsym(RTLD_NEXT,"SDL_GL_GetProcAddress");
    if (!next_getproc) {
        fputs("bt3-opengl: SDL3 GL loader unavailable\n",stderr);
        abort();
    }
#define LOAD(name) real_##name=(void *)next_getproc(#name)
    LOAD(glBindBuffer); LOAD(glBindBufferBase); LOAD(glBindBufferRange);
    LOAD(glBufferData); LOAD(glBufferSubData); LOAD(glDeleteBuffers); LOAD(glGetIntegerv);
    LOAD(glShaderSource); LOAD(glAttachShader); LOAD(glLinkProgram); LOAD(glUseProgram);
    LOAD(glDeleteProgram); LOAD(glDeleteShader); LOAD(glGetUniformIndices);
    LOAD(glGetActiveUniformsiv); LOAD(glGetActiveUniformBlockiv);
    LOAD(glDrawElements); LOAD(glDrawArrays); LOAD(glUniformBlockBinding);
#undef LOAD
    const char *simple=getenv("BT3_SOMBREADO_SIMPLE");
    if (simple && real_glGetUniformIndices && real_glGetActiveUniformsiv &&
        real_glGetActiveUniformBlockiv && real_glDrawElements && real_glDrawArrays)
        simple_shading=!strcmp(simple,"2") ? 2 : !strcmp(simple,"1") ? 1 : 0;
}

static void wrap_glBindBuffer(U target,U id) {
    if (target==UBO) bound_uniform=id;
    real_glBindBuffer(target,id);
}

static void bind_ranges(U id, UniformBuffer *b) {
    for (U j=0;j<BINDINGS;j++) if (indexed_uniforms[j]==id)
        real_glBindBufferRange(UBO,j,id,(ptrdiff_t)b->current,(ptrdiff_t)b->logical);
}

static void wrap_glBindBufferBase(U target,U index,U id) {
    if (target==UBO) {
        bound_uniform=id; /* Indexed binds also change the generic binding. */
        if (index<BINDINGS) indexed_uniforms[index]=id;
        if (index<BINDINGS && id && id<CAP && uniforms[id].shadow) {
            UniformBuffer *b=&uniforms[id];
            real_glBindBufferRange(target,index,id,(ptrdiff_t)b->current,(ptrdiff_t)b->logical);
            return;
        }
    }
    real_glBindBufferBase(target,index,id);
}

static void wrap_glBufferData(U target,ptrdiff_t size,const void *data,U usage) {
    U id=bound_uniform;
    if (target!=UBO || !id || id>=CAP || !real_glBindBufferRange) {
        real_glBufferData(target,size,data,usage); return;
    }
    UniformBuffer *b=&uniforms[id];
    free(b->shadow);
    memset(b,0,sizeof(*b));
    if (size<=0 || size>65536) { real_glBufferData(target,size,data,usage); return; }
    I align=0; real_glGetIntegerv(UBO_ALIGN,&align);
    if (align<=0) { real_glBufferData(target,size,data,usage); return; }
    b->logical=(size_t)size;
    b->stride=(b->logical+(size_t)align-1)/(size_t)align*(size_t)align;
    b->capacity=RING_BYTES;
    if (b->stride>b->capacity) { memset(b,0,sizeof(*b)); real_glBufferData(target,size,data,usage); return; }
    b->shadow=calloc(1,b->logical);
    if (!b->shadow) { memset(b,0,sizeof(*b)); real_glBufferData(target,size,data,usage); return; }
    if (data) memcpy(b->shadow,data,b->logical);
    real_glBufferData(target,(ptrdiff_t)b->capacity,NULL,STREAM_DRAW);
    if (data) {
        real_glBufferSubData(target,0,size,b->shadow);
        b->valid=1;
        b->next=b->stride;
    }
    bind_ranges(id,b);
}

static void flush_uniform(U id, UniformBuffer *b) {
    if (!b->dirty) return;
    if (simple_shading) real_glBindBuffer(UBO,id);
    if (b->next+b->stride>b->capacity) {
        real_glBufferData(UBO,(ptrdiff_t)b->capacity,NULL,STREAM_DRAW);
        b->next=0;
    }
    b->current=b->next;
    real_glBufferSubData(UBO,(ptrdiff_t)b->current,(ptrdiff_t)b->logical,b->shadow);
    b->next+=b->stride;
    b->valid=1; b->dirty=0;
    bind_ranges(id,b);
    if (!announced++) fputs("bt3-opengl: streaming uniform buffers enabled\n",stderr);
}

static void wrap_glBufferSubData(U target,ptrdiff_t offset,ptrdiff_t size,const void *data) {
    U id=bound_uniform;
    if (target!=UBO || !id || id>=CAP || !uniforms[id].shadow) {
        real_glBufferSubData(target,offset,size,data); return;
    }
    UniformBuffer *b=&uniforms[id];
    if (offset<0 || size<0 || (size_t)offset>b->logical || (size_t)size>b->logical-(size_t)offset || !data) {
        real_glBufferSubData(target,offset,size,data); return;
    }
    if (b->valid && !memcmp(b->shadow+(size_t)offset,data,(size_t)size)) return;
    memcpy(b->shadow+(size_t)offset,data,(size_t)size);
    b->valid=1; b->dirty=1;
    if (!simple_shading) flush_uniform(id,b);
}

static void flush_bound_uniforms(void) {
    for (U j=0;j<BINDINGS;j++) {
        U id=indexed_uniforms[j];
        if (id && id<CAP && uniforms[id].shadow && uniforms[id].dirty) flush_uniform(id,&uniforms[id]);
    }
    real_glBindBuffer(UBO,bound_uniform);
}

static void wrap_glUniformBlockBinding(U program,U block,U binding) {
    real_glUniformBlockBinding(program,block,binding);
    if (program<CAP) programs[program].checked=0;
}

static void wrap_glShaderSource(U shader,I count,const char *const *strings,const I *lengths) {
    if (simple_shading && shader<CAP && count>0 && strings) {
        size_t total=0;
        for (I j=0;j<count;j++) {
            if (!strings[j]) { total=0; break; }
            size_t n=lengths && lengths[j]>=0 ? (size_t)lengths[j] : strlen(strings[j]);
            if (n>1048576u || total>1048576u-n) { total=0; break; }
            total+=n;
        }
        fighter_shaders[shader]=0;
        if (total) {
            char *source=malloc(total+1);
            if (source) {
                size_t at=0;
                for (I j=0;j<count;j++) {
                    size_t n=lengths && lengths[j]>=0 ? (size_t)lengths[j] : strlen(strings[j]);
                    memcpy(source+at,strings[j],n); at+=n;
                }
                source[total]=0;
                fighter_shaders[shader]=strstr(source,"boneA") && strstr(source,"pivotA") && strstr(source,"u.misc.w");
                free(source);
            }
        }
    }
    real_glShaderSource(shader,count,strings,lengths);
}

static void wrap_glAttachShader(U program,U shader) {
    if (simple_shading && program<CAP && shader<CAP && fighter_shaders[shader]) programs[program].fighter=1;
    real_glAttachShader(program,shader);
}
static void wrap_glLinkProgram(U program) {
    if (program<CAP) programs[program].checked=0;
    real_glLinkProgram(program);
}
static void inspect_program(U program) {
    Program *p=&programs[program];
    p->checked=1; p->layer_offset=-1; p->binding=-1; p->screen_offset=-1;
    const char *names[]={"U.misc","u.misc","misc"};
    for (U j=0;j<3;j++) {
        U index=0xFFFFFFFFu;
        real_glGetUniformIndices(program,1,&names[j],&index);
        if (index==0xFFFFFFFFu) continue;
        I offset=-1,block=-1,type=0;
        real_glGetActiveUniformsiv(program,1,&index,0x8A3Bu,&offset); /* OFFSET */
        real_glGetActiveUniformsiv(program,1,&index,0x8A3Au,&block); /* BLOCK_INDEX */
        real_glGetActiveUniformsiv(program,1,&index,0x8A37u,&type); /* TYPE */
        if (offset<0 || offset>65520 || block<0 || type!=0x8B52) continue; /* FLOAT_VEC4 */
        I binding=-1;
        real_glGetActiveUniformBlockiv(program,(U)block,0x8A3Fu,&binding);
        if (binding>=0 && binding<(I)BINDINGS) {
            p->layer_offset=offset+12; p->binding=binding;
            const char *matrices[]={"U.screen","u.screen","screen"};
            for (U k=0;k<3;k++) {
                U matrix=0xFFFFFFFFu;
                real_glGetUniformIndices(program,1,&matrices[k],&matrix);
                if (matrix==0xFFFFFFFFu) continue;
                I mo=-1,mb=-1,mt=0,stride=0,row_major=1;
                real_glGetActiveUniformsiv(program,1,&matrix,0x8A3Bu,&mo);
                real_glGetActiveUniformsiv(program,1,&matrix,0x8A3Au,&mb);
                real_glGetActiveUniformsiv(program,1,&matrix,0x8A37u,&mt);
                real_glGetActiveUniformsiv(program,1,&matrix,0x8A3Du,&stride);
                real_glGetActiveUniformsiv(program,1,&matrix,0x8A3Eu,&row_major);
                if (mo>=0 && mo<=65472 && mb==block && mt==0x8B5C && stride==16 && !row_major) p->screen_offset=mo;
                break;
            }
            static int described;
            if (!described++) fprintf(stderr,"bt3-opengl: simple fighter shading enabled (uniform byte %d, binding %d)\n",p->layer_offset,binding);
            break;
        }
    }
}
static void wrap_glUseProgram(U program) {
    current_program=program;
    real_glUseProgram(program);
    if (simple_shading && program<CAP && programs[program].fighter && !programs[program].checked) inspect_program(program);
}
static void wrap_glDeleteProgram(U program) {
    if (program<CAP) memset(&programs[program],0,sizeof(Program));
    real_glDeleteProgram(program);
}
static void wrap_glDeleteShader(U shader) {
    if (shader<CAP) fighter_shaders[shader]=0;
    real_glDeleteShader(shader);
}
static int omit_toon_layer(void) {
    if (!simple_shading || !current_program || current_program>=CAP) return 0;
    Program *p=&programs[current_program];
    if (!p->fighter || !p->checked || p->layer_offset<0 || p->binding<0 || p->binding>=(I)BINDINGS) return 0;
    U id=indexed_uniforms[p->binding];
    if (!id || id>=CAP) return 0;
    UniformBuffer *b=&uniforms[id];
    if (!b->shadow || !b->valid || (size_t)p->layer_offset+sizeof(float)>b->logical) return 0;
    float layer;
    memcpy(&layer,b->shadow+p->layer_offset,sizeof(layer));
    if (layer==1.0f) return 1;
    if (simple_shading>=2 && layer==2.0f && p->screen_offset>=0 &&
        (size_t)p->screen_offset+16*sizeof(float)<=b->logical) {
        float matrix[16];
        memcpy(matrix,b->shadow+p->screen_offset,sizeof(matrix));
        /* Only the negative-w orthographic shadow camera, never a perspective silhouette. */
        return matrix[3]==0.0f && matrix[7]==0.0f && matrix[11]==0.0f && matrix[15]<0.0f;
    }
    return 0;
}
static void wrap_glDrawElements(U mode,I count,U type,const void *indices) {
    if (omit_toon_layer()) return;
    if (simple_shading) flush_bound_uniforms();
    real_glDrawElements(mode,count,type,indices);
}
static void wrap_glDrawArrays(U mode,I first,I count) {
    if (simple_shading) flush_bound_uniforms();
    real_glDrawArrays(mode,first,count);
}

static void wrap_glDeleteBuffers(I n,const U *ids) {
    for (I j=0;j<n;j++) {
        U id=ids[j];
        if (id<CAP) { free(uniforms[id].shadow); memset(&uniforms[id],0,sizeof(UniformBuffer)); }
        if (bound_uniform==id) bound_uniform=0;
        for (U k=0;k<BINDINGS;k++) if (indexed_uniforms[k]==id) indexed_uniforms[k]=0;
    }
    real_glDeleteBuffers(n,ids);
}

Proc SDL_GL_GetProcAddress(const char *name) {
    init();
#define HOOK(n) if (!strcmp(name,#n)) return (Proc)wrap_##n
    HOOK(glBindBuffer); HOOK(glBindBufferBase); HOOK(glBufferData);
    HOOK(glBufferSubData); HOOK(glDeleteBuffers);
    HOOK(glShaderSource); HOOK(glAttachShader); HOOK(glLinkProgram); HOOK(glUseProgram);
    HOOK(glDeleteProgram); HOOK(glDeleteShader); HOOK(glDrawElements); HOOK(glDrawArrays);
    HOOK(glUniformBlockBinding);
#undef HOOK
    return next_getproc(name);
}
