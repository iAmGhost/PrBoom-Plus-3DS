#include "gl_wrapper.h"
#include "gl_swizzle.h"

#include "../z_zone.h" // For malloc/free

#include <citro3d.h>
#include "vshader_shbin.h"

typedef struct _gl_c3d_tex {
    C3D_Tex c3d_tex;

    GLenum wrap_s;
    GLenum wrap_t;
    GLenum mag_filter;
    GLenum min_filter;

    int is_initialized;

    struct _gl_c3d_tex *prev;
    struct _gl_c3d_tex *next;
} gl_c3d_tex;

extern C3D_RenderTarget *hw_screen;

static u32 clear_color = 0x000000ff;
static u32 clear_depth = 0;

static int cull_enable = 0;
static int blend_enable = 0;
static int depth_enable = 0;
static int alpha_enable = 0;
static int scissor_enable = 0;

// src, dst
static GLenum cur_blend_factors[2] = { GL_ONE, GL_ZERO };

static GLenum cur_depth_func = GL_LESS;
static GLboolean cur_depth_mask = GL_TRUE;

static GLfloat cur_color[4];
static GLfloat cur_texcoord[4];

// Linked list of active GL texture objects
static gl_c3d_tex *gl_c3d_tex_base = NULL;
static gl_c3d_tex *gl_c3d_tex_head = NULL;

// Currently bound GL texture
static gl_c3d_tex *cur_texture = NULL;


static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uloc_mv_mtx, uloc_p_mtx, uloc_t_mtx;

static C3D_MtxStack mtx_modelview, mtx_projection, mtx_texture;
static C3D_MtxStack *cur_mtxstack;


void gl_wrapper_init() {
    // Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

    // Configure attributes for use with the vertex shader
	// Attribute format and element count are ignored in immediate mode
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 4); // v0=color
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 4); // v1=texcoord0
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 4); // v2=position

    C3D_CullFace(GPU_CULL_NONE);

    // Init matrix stacks
    MtxStack_Init(&mtx_modelview);
    uloc_mv_mtx = shaderInstanceGetUniformLocation(program.vertexShader, "mv_mtx");
    MtxStack_Bind(&mtx_modelview, GPU_VERTEX_SHADER, uloc_mv_mtx, 4);
    Mtx_Identity(MtxStack_Cur(&mtx_modelview));
    MtxStack_Update(&mtx_modelview);

    MtxStack_Init(&mtx_projection);
    uloc_p_mtx = shaderInstanceGetUniformLocation(program.vertexShader, "p_mtx");
    MtxStack_Bind(&mtx_projection, GPU_VERTEX_SHADER, uloc_p_mtx, 4);
    Mtx_Identity(MtxStack_Cur(&mtx_projection));
    MtxStack_Update(&mtx_projection);

    MtxStack_Init(&mtx_texture);
    uloc_t_mtx = shaderInstanceGetUniformLocation(program.vertexShader, "t_mtx");
    MtxStack_Bind(&mtx_texture, GPU_VERTEX_SHADER, uloc_t_mtx, 4);
    Mtx_Identity(MtxStack_Cur(&mtx_texture));
    MtxStack_Update(&mtx_texture);

    cur_mtxstack = &mtx_modelview;
}

void gl_wrapper_cleanup() {
    DVLB_Free(vshader_dvlb);
}

void gl_wrapper_perspective(float fovy, float aspect, float znear) {
    Mtx_PerspTilt(MtxStack_Cur(cur_mtxstack), fovy, 400.0f/240.0f, znear, 1000.0f, false);
}


//========== GRAPHICS FUNCTIONS ==========

void glEnable(GLenum cap) {
    /* switch(cap) {
    case GL_CULL_FACE:
        cull_enable = 1;
        break;
    case GL_BLEND:
        blend_enable = 1;
        break;
    case GL_DEPTH_TEST:
        depth_enable = 1;
        break;
    case GL_ALPHA_TEST:
        alpha_enable = 1;
        break;
    case GL_SCISSOR_TEST:
        scissor_enable = 1;
        break;
    } */
}

void glDisable(GLenum cap) {
    /* switch(cap) {
    case GL_CULL_FACE:
        cull_enable = 0;
        break;
    case GL_BLEND:
        blend_enable = 0;
        break;
    case GL_DEPTH_TEST:
        depth_enable = 0;
        break;
    case GL_ALPHA_TEST:
        alpha_enable = 0;
        break;
    case GL_SCISSOR_TEST:
        scissor_enable = 0;
        break;
    } */
}

void glCullFace(GLenum mode) {
    /* GPU_CULLMODE cull_mode = GPU_CULL_NONE;

    if(mode == GL_FRONT)
        cull_mode = GPU_CULL_FRONT_CCW;
    else if(mode == GL_BACK)
        cull_mode = GPU_CULL_BACK_CCW;

    C3D_CullFace(cull_enable ? cull_mode : GPU_CULL_NONE); */
}

void glBlendFunc(GLenum sfactor, GLenum dfactor) {
    /* cur_blend_factors[0] = sfactor;
    cur_blend_factors[1] = dfactor;

    GPU_BLENDFACTOR c3d_factors[2] = { GPU_ONE, GPU_ZERO };

    if(blend_enable)
    {
        for(int i = 0; i < 2; i++)
        {
            switch(cur_blend_factors[i])
            {
            case GL_ZERO: c3d_factors[i] = GPU_ZERO; break;
            case GL_ONE: c3d_factors[i] = GPU_ONE; break;
            case GL_SRC_COLOR: c3d_factors[i] = GPU_SRC_COLOR; break;
            case GL_ONE_MINUS_SRC_COLOR: c3d_factors[i] = GPU_ONE_MINUS_SRC_COLOR; break;
            case GL_SRC_ALPHA: c3d_factors[i] = GPU_SRC_ALPHA; break;
            case GL_ONE_MINUS_SRC_ALPHA: c3d_factors[i] = GPU_ONE_MINUS_SRC_ALPHA; break;
            case GL_DST_ALPHA: c3d_factors[i] = GPU_DST_ALPHA; break;
            case GL_ONE_MINUS_DST_ALPHA: c3d_factors[i] = GPU_ONE_MINUS_DST_ALPHA; break;
            case GL_DST_COLOR: c3d_factors[i] = GPU_DST_COLOR; break;
            case GL_ONE_MINUS_DST_COLOR: c3d_factors[i] = GPU_ONE_MINUS_DST_COLOR; break;
            case GL_SRC_ALPHA_SATURATE: c3d_factors[i] = GPU_SRC_ALPHA_SATURATE; break;
            }
        }
    }

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, c3d_factors[0], c3d_factors[1], c3d_factors[0], c3d_factors[1]); */
}

static inline GPU_TESTFUNC _gl_to_c3d_testfunc(GLenum gl_testfunc) {
    GPU_TESTFUNC ret = GPU_LESS;

    switch(gl_testfunc) {
    case GL_NEVER: ret = GPU_NEVER; break;
    case GL_LESS: ret = GPU_LESS; break;
    case GL_EQUAL: ret = GPU_EQUAL; break;
    case GL_LEQUAL: ret = GPU_LEQUAL; break;
    case GL_GREATER: ret = GPU_GREATER; break;
    case GL_NOTEQUAL: ret = GPU_NOTEQUAL; break;
    case GL_GEQUAL: ret = GPU_GEQUAL; break;
    case GL_ALWAYS: ret = GPU_ALWAYS; break;
    }

    return ret;
}

void glDepthFunc(GLenum func) {
    /* cur_depth_func = func;

    C3D_DepthTest(depth_enable, _gl_to_c3d_testfunc(cur_depth_func), cur_depth_mask ? GPU_WRITE_DEPTH : 0); */
}

void glDepthMask(GLboolean flag) {
    /* cur_depth_mask = flag;

    C3D_DepthTest(depth_enable, _gl_to_c3d_testfunc(cur_depth_func), cur_depth_mask ? GPU_WRITE_DEPTH : 0); */
}

void glAlphaFunc(GLenum func, GLclampf ref) {
    // C3D_AlphaTest(alpha_enable, _gl_to_c3d_testfunc(func), (int)(ref * 255.0f));
}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    clear_color = ((u32)(red * 255) << 24) | ((u32)(green * 255) << 16) | ((u32)(blue * 255) << 8) | (u32)(alpha * 255);
}

void glClearDepth(GLclampd depth) {
    clear_depth = (u32)(depth * 0xffff);
}

void glClear(GLbitfield mask) {
    C3D_ClearBits flags = 0;

    if(mask & GL_COLOR_BUFFER_BIT)
        flags |= C3D_CLEAR_COLOR;
    if(mask & GL_DEPTH_BUFFER_BIT)
        flags |= C3D_CLEAR_DEPTH;

    // FIXME: Why tf do the logs say "failed to allocate 2GBs of memory" and exits the
    // game when the depth clear value is set to anything other than 0??? >:(
    C3D_RenderTargetClear(hw_screen, flags, clear_color, 0);
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    C3D_SetViewport(y, x, height, width);
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    // C3D_SetScissor(scissor_enable ? GPU_SCISSOR_NORMAL : GPU_SCISSOR_DISABLE, y, x, height, width);
}

void glBegin(GLenum mode) {
    // TODO: Figure out if this is expensive
    MtxStack_Update(&mtx_modelview);
    MtxStack_Update(&mtx_projection);
    MtxStack_Update(&mtx_texture);

    if(cur_texture)
        C3D_TexBind(0, &cur_texture->c3d_tex);
    else
        C3D_TexBind(0, NULL);

    switch(mode) {
    case GL_TRIANGLE_STRIP:
        C3D_ImmDrawBegin(GPU_TRIANGLE_STRIP);
        break;
    case GL_TRIANGLE_FAN:
        C3D_ImmDrawBegin(GPU_TRIANGLE_FAN);
        break;
    default: // GL_TRIANGLES
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        break;
    }
}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    cur_color[0] = red;
    cur_color[1] = green;
    cur_color[2] = blue;
    cur_color[3] = alpha;
}

void glColor4ubv(const GLubyte *v) {
    cur_color[0] = v[0] * 255.0f;
    cur_color[1] = v[1] * 255.0f;
    cur_color[2] = v[2] * 255.0f;
    cur_color[3] = v[3] * 255.0f;
}

void glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    cur_color[0] = red;
    cur_color[1] = green;
    cur_color[2] = blue;
    cur_color[3] = 1.0f;
}

void glTexCoord2f(GLfloat s, GLfloat t) {
    cur_texcoord[0] = s;
    cur_texcoord[1] = t;
    cur_texcoord[2] = 0.0f;
    cur_texcoord[3] = 1.0f;
}

void glTexCoord2fv(const GLfloat *v) {
    cur_texcoord[0] = v[0];
    cur_texcoord[1] = v[1];
    cur_texcoord[2] = 0.0f;
    cur_texcoord[3] = 1.0f;
}

void glVertex2i(GLint x, GLint y) {
    cur_texcoord[0] = (GLfloat)x;
    cur_texcoord[1] = (GLfloat)y;
    cur_texcoord[2] = 0.0f;
    cur_texcoord[3] = 1.0f;
}

void glVertex2f(GLfloat x, GLfloat y) {
    C3D_ImmSendAttrib(cur_color[0], cur_color[1], cur_color[2], cur_color[3]);
    C3D_ImmSendAttrib(cur_texcoord[0], cur_texcoord[1], cur_texcoord[2], cur_texcoord[3]);
    C3D_ImmSendAttrib(x, y, 0.0f, 1.0f);
}

void glVertex3f(GLfloat x,GLfloat y,GLfloat z) {
    C3D_ImmSendAttrib(cur_color[0], cur_color[1], cur_color[2], cur_color[3]);
    C3D_ImmSendAttrib(cur_texcoord[0], cur_texcoord[1], cur_texcoord[2], cur_texcoord[3]);
    C3D_ImmSendAttrib(x, y, z, 1.0f);
}

void glVertex3fv(const GLfloat *v) {
    C3D_ImmSendAttrib(cur_color[0], cur_color[1], cur_color[2], cur_color[3]);
    C3D_ImmSendAttrib(cur_texcoord[0], cur_texcoord[1], cur_texcoord[2], cur_texcoord[3]);
    C3D_ImmSendAttrib(v[0], v[1], v[2], 1.0f);
}

void glEnd(void) {
    C3D_ImmDrawEnd();
}

static inline GLuint _gen_texture() {
    if(!gl_c3d_tex_head) // Linked list has no entries?
    {
        gl_c3d_tex_head = malloc(sizeof(gl_c3d_tex));

        gl_c3d_tex_head->prev = NULL;
        gl_c3d_tex_head->next = NULL;

        gl_c3d_tex_base = gl_c3d_tex_head;
    }
    else // Create new entry at the head of the list
    {
        gl_c3d_tex_head->next = malloc(sizeof(gl_c3d_tex));

        gl_c3d_tex_head->next->prev = gl_c3d_tex_head;
        gl_c3d_tex_head->next->next = NULL;

        gl_c3d_tex_head = gl_c3d_tex_head->next;
    }

    gl_c3d_tex_head->is_initialized = 0;

    // The pointer to the new texture acts as the GL texture ID
    return (GLuint)gl_c3d_tex_head;
}

void glGenTextures(GLsizei n, GLuint *textures) {
    for(int i = 0; i < n; i++)
        textures[i] = _gen_texture();
}

static inline gl_c3d_tex *_try_find_texture(GLuint texture) {
    gl_c3d_tex *cur_entry = gl_c3d_tex_base;

    while(cur_entry) {
        if((GLuint)cur_entry == texture)
            return cur_entry;

        cur_entry = cur_entry->next;
    }

    return NULL;
}

void glBindTexture(GLenum target, GLuint texture) {
    if(!texture) {
        cur_texture = NULL;
        return;
    }

    // Find the corresponding C3D texture
    gl_c3d_tex *gl_bind_tex = _try_find_texture(texture);

    if(!gl_bind_tex)
        return;

    cur_texture = gl_bind_tex;
}

static inline GPU_TEXTURE_WRAP_PARAM _gl_to_c3d_texwrap(GLenum mode) {
    GPU_TEXTURE_WRAP_PARAM ret;

    switch(mode) {
    case GL_CLAMP:
        ret = GPU_CLAMP_TO_EDGE;
        break;
    default: // GL_REPEAT
        ret = GPU_REPEAT;
        break;
    }

    return ret;
}

static inline GPU_TEXTURE_FILTER_PARAM _gl_to_c3d_texfilter(GLenum mode) {
    GPU_TEXTURE_FILTER_PARAM ret;

    switch(mode) {
    case GL_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_NEAREST_MIPMAP_NEAREST:
        ret = GPU_NEAREST;
        break;
    default: // GL_LINEAR
        ret = GPU_LINEAR;
        break;
    }

    return ret;
}

void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if(!cur_texture)
        return;

    switch(pname) {
    case GL_TEXTURE_WRAP_S: cur_texture->wrap_s = param; break;
    case GL_TEXTURE_WRAP_T: cur_texture->wrap_t = param; break;
    case GL_TEXTURE_MAG_FILTER: cur_texture->mag_filter = param; break;
    case GL_TEXTURE_MIN_FILTER: cur_texture->min_filter = param; break;
    }
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {
    if(!cur_texture)
        return;

    if(cur_texture->is_initialized)
        C3D_TexDelete(&cur_texture->c3d_tex);

    C3D_TexInit(&cur_texture->c3d_tex, width, height, GPU_RGBA8);
    C3D_TexSetWrap(&cur_texture->c3d_tex, _gl_to_c3d_texwrap(cur_texture->wrap_s), _gl_to_c3d_texwrap(cur_texture->wrap_t));
    C3D_TexSetFilter(&cur_texture->c3d_tex, _gl_to_c3d_texfilter(cur_texture->mag_filter), _gl_to_c3d_texfilter(cur_texture->min_filter));

    cur_texture->is_initialized = 1;

    // We can safely leave now if no pixel data (NULL) was provided
    if(!pixels)
        return;

    u32 *swizzle_buf = malloc(width * height * 4);
    SwizzleTexBufferRGBA8((u32*)pixels, swizzle_buf, width, height);

    // Reverse the order of the color components
    for(int i = 0; i < width * height; i++)
    {
        swizzle_buf[i] = ((swizzle_buf[i] & 0xff000000) >> 24) |
                         ((swizzle_buf[i] & 0x00ff0000) >> 8) |
                         ((swizzle_buf[i] & 0x0000ff00) << 8) |
                         ((swizzle_buf[i] & 0x000000ff) << 24);
    }

    C3D_TexUpload(&cur_texture->c3d_tex, swizzle_buf);

    free(swizzle_buf);
}

void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    // u32 src_width, src_height;
    // u32 dst_size;

    // u32 *src = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &src_width, &src_height);
    // u32 *dst = C3D_Tex2DGetImagePtr(cur_texture, level, &dst_size);
}

static inline void _delete_texture(GLuint texture) {
    // Find the corresponding C3D texture
    gl_c3d_tex *gl_delete_tex = _try_find_texture(texture);

    if(!gl_delete_tex)
        return;

    if(gl_delete_tex->is_initialized)
        C3D_TexDelete(&gl_delete_tex->c3d_tex);

    if(gl_delete_tex == cur_texture)
        cur_texture = NULL;

    gl_c3d_tex *prev = gl_delete_tex->prev;
    gl_c3d_tex *next = gl_delete_tex->next;

    // Delete (free) the texture
    free(gl_delete_tex);

    // The deleted texture was...
    if(!prev && next) // ...at the start of the list
    {
        next->prev = NULL;
        gl_c3d_tex_base = next;
    }
    else if(prev && next) // ...between 2 list entries
    {
        prev->next = next;
        next->prev = prev;
    }
    else if(prev && !next) // ...at the end of the list
    {
        prev->next = NULL;
        gl_c3d_tex_head = prev;
    }
    else // ...the only entry in the list
    {
        gl_c3d_tex_base = NULL;
        gl_c3d_tex_head = NULL;
    }
}

void glDeleteTextures(GLsizei n, const GLuint *textures) {
    for(int i = 0; i < n; i++)
        _delete_texture(textures[i]);
}

void glFogi(GLenum pname, GLint param) {

}

void glFogf(GLenum pname, GLfloat param) {

}

void glFogfv(GLenum pname, const GLfloat *params) {

}

void glTexEnvi(GLenum target, GLenum pname, GLint param) {
    // Configure the first fragment shading substage to just pass through the vertex color
    //C3D_TexEnv* env = C3D_GetTexEnv(0);
	//C3D_TexEnvInit(env);
	//C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
	//C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
}

void glGetIntegerv(GLenum pname, GLint *params) {
    switch(pname) {
    case GL_BLEND_DST:
        params[0] = cur_blend_factors[1];
        break;
    case GL_BLEND_SRC:
        params[0] = cur_blend_factors[0];
        break;
    }
}

void glGetFloatv(GLenum pname, GLfloat *params) {
    switch(pname) {
    case GL_CURRENT_COLOR:
        params[0] = cur_color[0];
        params[1] = cur_color[1];
        params[2] = cur_color[2];
        params[3] = cur_color[3];
        break;
    }
}

void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {
    if(!cur_texture)
        return;

    switch(pname) {
    case GL_TEXTURE_WIDTH:
        params[0] = cur_texture->c3d_tex.width;
        break;
    case GL_TEXTURE_HEIGHT:
        params[0] = cur_texture->c3d_tex.height;
        break;
    }
}

void glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels) {

}

void glFlush(void) {

}

void glFinish(void) {

}


//========== MATRIX FUNCTIONS ==========

void glMatrixMode(GLenum mode) {
    switch(mode) {
    case GL_MODELVIEW:
        cur_mtxstack = &mtx_modelview;
        break;
    case GL_PROJECTION:
        cur_mtxstack = &mtx_projection;
        break;
    case GL_TEXTURE:
        cur_mtxstack = &mtx_texture;
        break;
    }
}

void glLoadIdentity(void) {
    Mtx_Identity(MtxStack_Cur(cur_mtxstack));
}

void glLoadMatrixf(const GLfloat *m) {
    float *out = MtxStack_Cur(cur_mtxstack)->m;

    // (3DS GPU expects matrix row permutation to be WZYX, not XYZW!)
    out[0] = m[12]; out[1] = m[8]; out[2] = m[4]; out[3] = m[0];
    out[4] = m[13]; out[5] = m[9]; out[6] = m[5]; out[7] = m[1];
    out[8] = m[14]; out[9] = m[10]; out[10] = m[6]; out[11] = m[2];
    out[12] = m[15]; out[13] = m[11]; out[14] = m[7]; out[15] = m[3];
}

void glPushMatrix(void) {
    MtxStack_Push(cur_mtxstack);
}

void glPopMatrix(void) {
    MtxStack_Pop(cur_mtxstack);
}

void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    Mtx_OrthoTilt(MtxStack_Cur(cur_mtxstack), (float)left, (float)right, (float)bottom, (float)top, (float)zNear, (float)zFar, false);
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    Mtx_Translate(MtxStack_Cur(cur_mtxstack), x, y, z, true);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    C3D_FVec axis = { x, y, z };
    Mtx_Rotate(MtxStack_Cur(cur_mtxstack), axis, angle, true);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    Mtx_Scale(MtxStack_Cur(cur_mtxstack), x, y, z);
}
