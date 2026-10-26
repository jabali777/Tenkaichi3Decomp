/* CPU-only regression checks: streaming, deferred writes and selective shading. */
#include "bt3_opengl_fast.c"
#include <assert.h>

static unsigned char *stores[CAP];
static size_t sizes[CAP], range_offset[BINDINGS], range_size[BINDINGS];
static U actual_binding, range_id[BINDINGS], last_target;
static int allocations, uploads, deletes, drawn_elements, drawn_arrays, shader_calls;
static I block_binding;
static void fake_bind(U target,U id) { if (target==UBO) actual_binding=id; }
static void fake_base(U target,U index,U id) { (void)index; if (target==UBO) actual_binding=id; }
static void fake_range(U target,U index,U id,ptrdiff_t offset,ptrdiff_t size) {
    assert(target==UBO && index<BINDINGS && id<CAP);
    assert(offset>=0 && (size_t)offset%256==0);
    assert(size>0 && (size_t)offset+(size_t)size<=sizes[id]);
    actual_binding=id; range_id[index]=id;
    range_offset[index]=(size_t)offset; range_size[index]=(size_t)size;
}
static void fake_data(U target,ptrdiff_t size,const void *data,U usage) {
    (void)usage; last_target=target; allocations++;
    if (target!=UBO) return;
    assert(actual_binding<CAP && size>0);
    free(stores[actual_binding]);
    stores[actual_binding]=calloc(1,(size_t)size);
    assert(stores[actual_binding]); sizes[actual_binding]=(size_t)size;
    if (data) memcpy(stores[actual_binding],data,(size_t)size);
}
static void fake_sub(U target,ptrdiff_t offset,ptrdiff_t size,const void *data) {
    last_target=target; uploads++;
    assert(target==UBO && actual_binding<CAP && offset>=0 && size>=0);
    assert((size_t)offset+(size_t)size<=sizes[actual_binding]);
    memcpy(stores[actual_binding]+(size_t)offset,data,(size_t)size);
}
static void fake_delete(I n,const U *ids) {
    deletes+=n;
    for (I j=0;j<n;j++) if (ids[j]<CAP) {
        free(stores[ids[j]]); stores[ids[j]]=NULL; sizes[ids[j]]=0;
        if (actual_binding==ids[j]) actual_binding=0;
    }
}
static void fake_get(U parameter,I *value) { assert(parameter==UBO_ALIGN); *value=256; }
static void fake_shader(U id,I n,const char *const *source,const I *length) {
    (void)id; (void)n; (void)source; (void)length; shader_calls++;
}
static void fake_attach(U program,U shader) { (void)program; (void)shader; }
static void fake_program(U program) { (void)program; }
static void fake_indices(U program,I count,const char *const *names,U *indices) {
    assert(program==5 && count==1);
    indices[0]=!strcmp(names[0],"U.misc") ? 3 : !strcmp(names[0],"U.screen") ? 4 : 0xFFFFFFFFu;
}
static void fake_uniform_info(U program,I count,const U *index,U parameter,I *value) {
    assert(program==5 && count==1 && (index[0]==3 || index[0]==4));
    switch (parameter) {
        case 0x8A3B: *value=index[0]==3 ? 272 : 160; break;
        case 0x8A3A: *value=0; break;
        case 0x8A37: *value=index[0]==3 ? 0x8B52 : 0x8B5C; break;
        case 0x8A3D: *value=16; break;
        case 0x8A3E: *value=0; break;
        default: abort();
    }
}
static void fake_block_info(U program,U block,U parameter,I *value) {
    assert(program==5 && block==0 && parameter==0x8A3F); *value=block_binding;
}
static void fake_block_binding(U program,U block,U binding) {
    assert(program==5 && block==0); block_binding=(I)binding;
}
static void fake_elements(U mode,I count,U type,const void *indices) {
    (void)mode; (void)count; (void)type; (void)indices; drawn_elements++;
}
static void fake_arrays(U mode,I first,I count) { (void)mode; (void)first; (void)count; drawn_arrays++; }

int main(void) {
    real_glBindBuffer=fake_bind; real_glBindBufferBase=fake_base;
    real_glBindBufferRange=fake_range; real_glBufferData=fake_data;
    real_glBufferSubData=fake_sub; real_glDeleteBuffers=fake_delete;
    real_glGetIntegerv=fake_get;
    unsigned char original[80], changed[16];
    memset(original,0xAA,sizeof(original)); memset(changed,0xBB,sizeof(changed));
    wrap_glBindBuffer(UBO,7); wrap_glBufferData(UBO,80,NULL,STREAM_DRAW);
    assert(sizes[7]==RING_BYTES && allocations==1);
    wrap_glBindBufferBase(UBO,1,7);
    wrap_glBufferSubData(UBO,0,80,original);
    assert(range_id[1]==7 && range_size[1]==80 && range_offset[1]==0 && uploads==1);
    wrap_glBufferSubData(UBO,0,80,original);
    assert(uploads==1 && uniforms[7].next==256);
    wrap_glBufferSubData(UBO,16,16,changed);
    assert(uploads==2 && range_offset[1]==256);
    assert(!memcmp(stores[7],original,80));
    assert(!memcmp(stores[7]+256,original,16));
    assert(!memcmp(stores[7]+272,changed,16));
    assert(!memcmp(stores[7]+288,original+32,48));
    wrap_glBufferSubData(UBO,16,16,changed); assert(uploads==2);
    uniforms[7].next=uniforms[7].capacity-uniforms[7].stride;
    changed[0]=0xCC; wrap_glBufferSubData(UBO,16,16,changed);
    assert(range_offset[1]==RING_BYTES-256 && allocations==1);
    changed[0]=0xDD; wrap_glBufferSubData(UBO,16,16,changed);
    assert(allocations==2 && range_offset[1]==0);
    assert(!memcmp(stores[7],original,16) && !memcmp(stores[7]+16,changed,16));
    wrap_glBindBufferBase(UBO,2,7);
    U id=7; wrap_glDeleteBuffers(1,&id);
    assert(deletes==1 && !uniforms[7].shadow && !bound_uniform && !indexed_uniforms[1] && !indexed_uniforms[2]);
    wrap_glBindBuffer(0x8892u,42); wrap_glBufferData(0x8892u,80,original,STREAM_DRAW);
    assert(last_target==0x8892u);

    real_glShaderSource=fake_shader; real_glAttachShader=fake_attach;
    real_glLinkProgram=fake_program; real_glUseProgram=fake_program;
    real_glDeleteProgram=fake_program; real_glDeleteShader=fake_program;
    real_glGetUniformIndices=fake_indices; real_glGetActiveUniformsiv=fake_uniform_info;
    real_glGetActiveUniformBlockiv=fake_block_info; real_glUniformBlockBinding=fake_block_binding;
    real_glDrawElements=fake_elements; real_glDrawArrays=fake_arrays;
    simple_shading=1;
    const char *parts[]={"boneA; ","pivotA; u.misc.w;"};
    wrap_glShaderSource(11,2,parts,NULL);
    assert(shader_calls==1 && fighter_shaders[11]);
    wrap_glAttachShader(5,11); wrap_glLinkProgram(5); wrap_glUseProgram(5);
    assert(programs[5].fighter && programs[5].layer_offset==284 && programs[5].screen_offset==160);
    wrap_glUniformBlockBinding(5,0,1); assert(!programs[5].checked);
    wrap_glUseProgram(5); assert(programs[5].binding==1);
    wrap_glUniformBlockBinding(5,0,0); wrap_glUseProgram(5);
    float vertex[80]={0}; vertex[71]=1.0f; vertex[55]=-862.0f;
    wrap_glBindBuffer(UBO,7); wrap_glBufferData(UBO,sizeof(vertex),NULL,STREAM_DRAW);
    wrap_glBindBufferBase(UBO,0,7);
    wrap_glBindBuffer(UBO,8); wrap_glBufferData(UBO,sizeof(original),NULL,STREAM_DRAW);
    wrap_glBindBufferBase(UBO,1,8);
    int before=uploads;
    wrap_glBindBuffer(UBO,7); wrap_glBufferSubData(UBO,0,sizeof(vertex),vertex);
    wrap_glBindBuffer(UBO,8); wrap_glBufferSubData(UBO,0,sizeof(original),original);
    assert(uploads==before && uniforms[7].dirty && uniforms[8].dirty);
    wrap_glDrawElements(4,3,0x1405u,NULL); assert(drawn_elements==0 && uploads==before);
    vertex[71]=0.0f; wrap_glBindBuffer(UBO,7); wrap_glBufferSubData(UBO,0,sizeof(vertex),vertex);
    wrap_glDrawElements(4,3,0x1405u,NULL);
    assert(drawn_elements==1 && uploads==before+2 && actual_binding==bound_uniform);
    wrap_glDrawElements(4,3,0x1405u,NULL); assert(drawn_elements==2 && uploads==before+2);
    vertex[71]=2.0f; wrap_glBufferSubData(UBO,0,sizeof(vertex),vertex);
    wrap_glDrawElements(4,3,0x1405u,NULL); assert(drawn_elements==3); /* Shadow retained in mode 1. */
    simple_shading=2;
    wrap_glDrawElements(4,3,0x1405u,NULL); assert(drawn_elements==3); /* Orthographic shadow omitted. */
    vertex[51]=1.0f; wrap_glBufferSubData(UBO,0,sizeof(vertex),vertex);
    wrap_glDrawElements(4,3,0x1405u,NULL); assert(drawn_elements==4); /* Perspective silhouette retained. */
    vertex[51]=0.0f; vertex[55]=862.0f; wrap_glBufferSubData(UBO,0,sizeof(vertex),vertex);
    wrap_glDrawElements(4,3,0x1405u,NULL); assert(drawn_elements==5); /* Other cameras retained. */
    vertex[71]=5.0f; wrap_glBufferSubData(UBO,0,sizeof(vertex),vertex);
    wrap_glDrawElements(4,3,0x1405u,NULL); assert(drawn_elements==6); /* Reflection layer retained. */
    vertex[71]=1.0f; wrap_glBufferSubData(UBO,0,sizeof(vertex),vertex);
    programs[5].layer_offset=10000;
    wrap_glDrawElements(4,3,0x1405u,NULL); assert(drawn_elements==7); /* Unknown layout fails open. */
    programs[5].layer_offset=284;
    wrap_glUseProgram(10);
    wrap_glDrawElements(4,3,0x1405u,NULL); assert(drawn_elements==8); /* Other shaders retained. */
    wrap_glUseProgram(5);
    wrap_glDrawArrays(4,0,3); assert(drawn_arrays==1); /* HUD / native passes never omitted. */
    U ids[]={7,8}; wrap_glDeleteBuffers(2,ids);
    wrap_glDeleteProgram(5); wrap_glDeleteShader(11);
    assert(!programs[5].fighter && !fighter_shaders[11]);
    for (U j=0;j<CAP;j++) assert(!stores[j] && !uniforms[j].shadow);
    puts("streaming + simple shading: all checks passed");
    return 0;
}
