/*
* Copyright (C) 2011 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include "FrameBuffer.h"
#include "NativeSubWindow.h"
#include "FBConfig.h"
#include "EGLDispatch.h"
#include "GLDispatch.h"
#include "GL2Dispatch.h"
#include "ThreadInfo.h"
#include <stdio.h>
#include "TimeUtils.h"

FrameBuffer *FrameBuffer::s_theFrameBuffer = NULL;
HandleType FrameBuffer::s_nextHandle = 0;

#ifdef WITH_GLES2
static const char *getGLES2ExtensionString(EGLDisplay p_dpy)
{
    EGLConfig config;
    EGLSurface surface;

    GLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    int n;
    if (!s_egl.eglChooseConfig(p_dpy, configAttribs,
                               &config, 1, &n)) {
        return NULL;
    }

    EGLint pbufAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };

    surface = s_egl.eglCreatePbufferSurface(p_dpy, config, pbufAttribs);
    if (surface == EGL_NO_SURFACE) {
        return NULL;
    }

    GLint gl2ContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLContext ctx = s_egl.eglCreateContext(p_dpy, config,
                                            EGL_NO_CONTEXT,
                                            gl2ContextAttribs);
    if (ctx == EGL_NO_CONTEXT) {
        s_egl.eglDestroySurface(p_dpy, surface);
        return NULL;
    }

    if (!s_egl.eglMakeCurrent(p_dpy, surface, surface, ctx)) {
        s_egl.eglDestroySurface(p_dpy, surface);
        s_egl.eglDestroyContext(p_dpy, ctx);
        return NULL;
    }

    const char *extString = (const char *)s_gl2.glGetString(GL_EXTENSIONS);
    if (!extString) {
        extString = "";
    }

    s_egl.eglMakeCurrent(p_dpy, NULL, NULL, NULL);
    s_egl.eglDestroyContext(p_dpy, ctx);
    s_egl.eglDestroySurface(p_dpy, surface);

    return extString;
}
#endif

void FrameBuffer::finalize() {
    if(s_theFrameBuffer){
        s_theFrameBuffer->removeSubWindow();
        s_theFrameBuffer->m_colorbuffers.clear();
        s_theFrameBuffer->m_windows.clear();
        s_theFrameBuffer->m_contexts.clear();
        s_egl.eglMakeCurrent(s_theFrameBuffer->m_eglDisplay, NULL, NULL, NULL);
        s_egl.eglDestroyContext(s_theFrameBuffer->m_eglDisplay,s_theFrameBuffer->m_eglContext);
        s_egl.eglDestroyContext(s_theFrameBuffer->m_eglDisplay,s_theFrameBuffer->m_pbufContext);
        s_egl.eglDestroySurface(s_theFrameBuffer->m_eglDisplay,s_theFrameBuffer->m_pbufSurface);
        s_theFrameBuffer = NULL;
    }
}

bool FrameBuffer::initialize(int width, int height, OnPostFn onPost, void* onPostContext)
{
    if (s_theFrameBuffer != NULL) {
        return true;
    }

    //
    // allocate space for the FrameBuffer object
    //
    FrameBuffer *fb = new FrameBuffer(width, height, onPost, onPostContext);
    if (!fb) {
        ERR("Failed to create fb\n");
        return false;
    }

#ifdef WITH_GLES2
    //
    // Try to load GLES2 Plugin, not mandatory
    //
    if (getenv("ANDROID_NO_GLES2")) {
        fb->m_caps.hasGL2 = false;
    }
    else {
        fb->m_caps.hasGL2 = s_gl2_enabled;
    }
#else
    fb->m_caps.hasGL2 = false;
#endif

    //
    // Initialize backend EGL display
    //
    fb->m_eglDisplay = s_egl.eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (fb->m_eglDisplay == EGL_NO_DISPLAY) {
        ERR("Failed to Initialize backend EGL display\n");
        delete fb;
        return false;
    }

    if (!s_egl.eglInitialize(fb->m_eglDisplay, &fb->m_caps.eglMajor, &fb->m_caps.eglMinor)) {
        ERR("Failed to eglInitialize\n");
        delete fb;
        return false;
    }

    DBG("egl: %d %d\n", fb->m_caps.eglMajor, fb->m_caps.eglMinor);
    s_egl.eglBindAPI(EGL_OPENGL_ES_API);

    //
    // if GLES2 plugin has loaded - try to make GLES2 context and
    // get GLES2 extension string
    //
    const char *gl2Extensions = NULL;
#ifdef WITH_GLES2
    if (fb->m_caps.hasGL2) {
        gl2Extensions = getGLES2ExtensionString(fb->m_eglDisplay);
        if (!gl2Extensions) {
            // Could not create GLES2 context - drop GL2 capability
            fb->m_caps.hasGL2 = false;
        }
    }
#endif

    //
    // Create EGL context for framebuffer post rendering.
    //
#if 0
    GLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_NONE
    };
#else
    GLint configAttribs[] = {
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_NONE
    };
#endif

    int n;
    if (!s_egl.eglChooseConfig(fb->m_eglDisplay, configAttribs,
                               &fb->m_eglConfig, 1, &n)) {
        ERR("Failed on eglChooseConfig\n");
        delete fb;
        return false;
    }

    GLint glContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 1,
        EGL_NONE
    };

    fb->m_eglContext = s_egl.eglCreateContext(fb->m_eglDisplay, fb->m_eglConfig,
                                              EGL_NO_CONTEXT,
                                              glContextAttribs);
    if (fb->m_eglContext == EGL_NO_CONTEXT) {
        printf("Failed to create Context 0x%x\n", s_egl.eglGetError());
        delete fb;
        return false;
    }

    //
    // Create another context which shares with the eglContext to be used
    // when we bind the pbuffer. That prevent switching drawable binding
    // back and forth on framebuffer context.
    // The main purpose of it is to solve a "blanking" behaviour we see on
    // on Mac platform when switching binded drawable for a context however
    // it is more efficient on other platforms as well.
    //
    fb->m_pbufContext = s_egl.eglCreateContext(fb->m_eglDisplay, fb->m_eglConfig,
                                               fb->m_eglContext,
                                               glContextAttribs);
    if (fb->m_pbufContext == EGL_NO_CONTEXT) {
        printf("Failed to create Pbuffer Context 0x%x\n", s_egl.eglGetError());
        delete fb;
        return false;
    }

    //
    // create a 1x1 pbuffer surface which will be used for binding
    // the FB context.
    // The FB output will go to a subwindow, if one exist.
    //
    EGLint pbufAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };

    fb->m_pbufSurface = s_egl.eglCreatePbufferSurface(fb->m_eglDisplay,
                                                      fb->m_eglConfig,
                                                      pbufAttribs);
    if (fb->m_pbufSurface == EGL_NO_SURFACE) {
        printf("Failed to create pbuf surface for FB 0x%x\n", s_egl.eglGetError());
        delete fb;
        return false;
    }

    // Make the context current
    if (!fb->bind_locked()) {
        ERR("Failed to make current\n");
        delete fb;
        return false;
    }

    //
    // Initilize framebuffer capabilities
    //
    const char *glExtensions = (const char *)s_gl.glGetString(GL_EXTENSIONS);
    bool has_gl_oes_image = false;
    if (glExtensions) {
        has_gl_oes_image = strstr(glExtensions, "GL_OES_EGL_image") != NULL;
    }

    if (fb->m_caps.hasGL2 && has_gl_oes_image) {
        has_gl_oes_image &= (strstr(gl2Extensions, "GL_OES_EGL_image") != NULL);
    }

    const char *eglExtensions = s_egl.eglQueryString(fb->m_eglDisplay,
                                                     EGL_EXTENSIONS);

    if (eglExtensions && has_gl_oes_image) {
        fb->m_caps.has_eglimage_texture_2d =
                strstr(eglExtensions, "EGL_KHR_gl_texture_2D_image") != NULL;
        fb->m_caps.has_eglimage_renderbuffer =
                strstr(eglExtensions, "EGL_KHR_gl_renderbuffer_image") != NULL;
    }
    else {
        fb->m_caps.has_eglimage_texture_2d = false;
        fb->m_caps.has_eglimage_renderbuffer = false;
    }

    //
    // Fail initialization if not all of the following extensions
    // exist:
    //     EGL_KHR_gl_texture_2d_image
    //     GL_OES_EGL_IMAGE (by both GLES implementations [1 and 2])
    //
    if (!fb->m_caps.has_eglimage_texture_2d) {
        ERR("Failed: Missing egl_image related extension(s)\n");
        delete fb;
        return false;
    }

    //
    // Initialize set of configs
    //
    InitConfigStatus configStatus = FBConfig::initConfigList(fb);
    if (configStatus == INIT_CONFIG_FAILED) {
        ERR("Failed: Initialize set of configs\n");
        delete fb;
        return false;
    }

    //
    // Check that we have config for each GLES and GLES2
    //
    int nConfigs = FBConfig::getNumConfigs();
    int nGLConfigs = 0;
    int nGL2Configs = 0;
    for (int i=0; i<nConfigs; i++) {
        GLint rtype = FBConfig::get(i)->getRenderableType();
        if (0 != (rtype & EGL_OPENGL_ES_BIT)) {
            nGLConfigs++;
        }
        if (0 != (rtype & EGL_OPENGL_ES2_BIT)) {
            nGL2Configs++;
        }
    }

    //
    // Fail initialization if no GLES configs exist
    //
    if (nGLConfigs == 0) {
        delete fb;
        return false;
    }

    //
    // If no GLES2 configs exist - not GLES2 capability
    //
    if (nGL2Configs == 0) {
        fb->m_caps.hasGL2 = false;
    }

    //
    // Initialize some GL state in the pbuffer context
    //
    fb->initGLState(width, height);

    //
    // Allocate space for the onPost framebuffer image
    //
    if (onPost) {
        fb->m_fbImage = (unsigned char*)malloc(4 * width * height);
        if (!fb->m_fbImage) {
            ERR("Failed to allocate space for onPost framebuffer image\n");
            delete fb;
            return false;
        }

        // Create framebuffer of fixed size
        GLuint renderbuffer;
        s_gl.glGenRenderbuffersOES(1, &renderbuffer);
        s_gl.glBindRenderbufferOES(GL_RENDERBUFFER_OES, renderbuffer);
        s_gl.glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_RGBA8_OES, width, height);

        s_gl.glGenFramebuffersOES(1, &fb->m_framebuffer);
        s_gl.glBindFramebufferOES(GL_FRAMEBUFFER_OES, fb->m_framebuffer);
        s_gl.glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES,
                                          GL_RENDERBUFFER_OES, renderbuffer);
    }

    // Force VSync
    s_egl.eglSwapInterval(fb->m_eglDisplay, 1);

    // release the FB context
    fb->unbind_locked();

    // set T0
    fb->m_statsStartTime = GetCurrentTimeMS() + 7000; // 7 seconds

    //
    // Keep the singleton framebuffer pointer
    //
    s_theFrameBuffer = fb;
    return true;
}

FrameBuffer::FrameBuffer(int p_width, int p_height,
                         OnPostFn onPost, void* onPostContext) :
    m_x(0.0f),
    m_y(0.0f),
    m_width(p_width),
    m_height(p_height),
    m_FBwidth(p_width),
    m_FBheight(p_height),
    m_eglDisplay(EGL_NO_DISPLAY),
    m_eglSurface(EGL_NO_SURFACE),
    m_eglContext(EGL_NO_CONTEXT),
    m_pbufContext(EGL_NO_CONTEXT),
    m_prevContext(EGL_NO_CONTEXT),
    m_prevReadSurf(EGL_NO_SURFACE),
    m_prevDrawSurf(EGL_NO_SURFACE),
    m_subWin(NULL),
    m_subWinDisplay(NULL),
    m_lastPostedColorBuffer(0),
    m_zRot(0.0f),
    m_eglContextInitialized(false),
    m_statsNumFrames(0),
    m_statsStartTime(0LL),
    m_onPost(onPost),
    m_onPostContext(onPostContext),
    m_fbImage(NULL),
    m_framebuffer(0),
    m_textLogo(0),
    m_logoRatio(0),
    m_textStartScreeen(0),
    m_windowHighlight(false)
{
    m_fpsStats = getenv("SHOW_FPS_STATS") != NULL;
}

void FrameBuffer::setDisplayRotation(float zRot)
{
    int rot = (int)(zRot - m_zRot);

    if (rot == 90 || rot == -90) {
        int tmp = m_FBwidth;
        m_FBwidth = m_FBheight;
        m_FBheight = tmp;
    }
    m_zRot = zRot;

    repost();
}

FrameBuffer::~FrameBuffer()
{
    free(m_fbImage);
}

bool FrameBuffer::setupSubWindow(FBNativeWindowType p_window,
                                 int p_x, int p_y,
                                 int p_width, int p_height, float zRot)
{
    bool success = false;

    if (s_theFrameBuffer) {
        s_theFrameBuffer->m_lock.lock();
        FrameBuffer *fb = s_theFrameBuffer;
        if (!fb->m_subWin) {

            // create native subwindow for FB display output
            fb->m_subWin = createSubWindow(p_window,
                                           &fb->m_subWinDisplay,
                                           p_x,p_y,p_width,p_height);
            if (fb->m_subWin) {
                fb->m_nativeWindow = p_window;

                // create EGLSurface from the generated subwindow
                fb->m_eglSurface = s_egl.eglCreateWindowSurface(fb->m_eglDisplay,
                                                                fb->m_eglConfig,
                                                                fb->m_subWin,
                                                                NULL);

                if (fb->m_eglSurface == EGL_NO_SURFACE) {
                    ERR("Failed to create surface\n");
                    destroySubWindow(fb->m_subWinDisplay, fb->m_subWin);
                    fb->m_subWin = NULL;
                }
                else if (fb->bindSubwin_locked()) {
                    // Subwin creation was successfull,
                    // update viewport and z rotation and draw
                    // the last posted color buffer.
                    s_gl.glViewport(0, 0, p_width, p_height);
                    fb->m_zRot = zRot;
                    fb->m_FBwidth = p_width;
                    fb->m_FBheight = p_height;
                    fb->post( fb->m_lastPostedColorBuffer, false );

                    fb->unbind_locked();
                    success = true;
                }
            }
        }
        s_theFrameBuffer->m_lock.unlock();
    }

    return success;
}

bool FrameBuffer::removeSubWindow()
{
    bool removed = false;
    if (s_theFrameBuffer) {
        s_theFrameBuffer->m_lock.lock();
        if (s_theFrameBuffer->m_subWin) {
            s_egl.eglMakeCurrent(s_theFrameBuffer->m_eglDisplay, NULL, NULL, NULL);
            s_egl.eglDestroySurface(s_theFrameBuffer->m_eglDisplay,
                                    s_theFrameBuffer->m_eglSurface);
            destroySubWindow(s_theFrameBuffer->m_subWinDisplay,
                             s_theFrameBuffer->m_subWin);

            s_theFrameBuffer->m_eglSurface = EGL_NO_SURFACE;
            s_theFrameBuffer->m_subWin = NULL;
            removed = true;
        }
        s_theFrameBuffer->m_lock.unlock();
    }
    return removed;
}

HandleType FrameBuffer::genHandle()
{
    HandleType id;
    do {
        id = ++s_nextHandle;
    } while( id == 0 ||
             m_contexts.find(id) != m_contexts.end() ||
             m_windows.find(id) != m_windows.end() );

    return id;
}

HandleType FrameBuffer::createColorBuffer(int p_width, int p_height,
                                          GLenum p_internalFormat)
{
    android::Mutex::Autolock mutex(m_lock);
    HandleType ret = 0;

    ColorBufferPtr cb( ColorBuffer::create(p_width, p_height, p_internalFormat) );
    if (cb.Ptr() != NULL) {
        ret = genHandle();
        m_colorbuffers[ret].cb = cb;
        m_colorbuffers[ret].refcount = 1;
    }
    return ret;
}

HandleType FrameBuffer::createRenderContext(int p_config, HandleType p_share,
                                            bool p_isGL2)
{
    android::Mutex::Autolock mutex(m_lock);
    HandleType ret = 0;

    RenderContextPtr share(NULL);
    if (p_share != 0) {
        RenderContextMap::iterator s( m_contexts.find(p_share) );
        if (s == m_contexts.end()) {
            return 0;
        }
        share = (*s).second;
    }

    RenderContextPtr rctx( RenderContext::create(p_config, share, p_isGL2) );
    if (rctx.Ptr() != NULL) {
        ret = genHandle();
        m_contexts[ret] = rctx;
    }
    return ret;
}

HandleType FrameBuffer::createWindowSurface(int p_config, int p_width, int p_height)
{
    android::Mutex::Autolock mutex(m_lock);

    HandleType ret = 0;
    WindowSurfacePtr win( WindowSurface::create(p_config, p_width, p_height) );
    if (win.Ptr() != NULL) {
        ret = genHandle();
        m_windows[ret] = win;
    }

    return ret;
}

void FrameBuffer::DestroyRenderContext(HandleType p_context)
{
    android::Mutex::Autolock mutex(m_lock);
    m_contexts.erase(p_context);
}

void FrameBuffer::DestroyWindowSurface(HandleType p_surface)
{
    android::Mutex::Autolock mutex(m_lock);
    m_windows.erase(p_surface);
}

void FrameBuffer::openColorBuffer(HandleType p_colorbuffer)
{
    android::Mutex::Autolock mutex(m_lock);
    ColorBufferMap::iterator c(m_colorbuffers.find(p_colorbuffer));
    if (c == m_colorbuffers.end()) {
        // bad colorbuffer handle
        return;
    }
    (*c).second.refcount++;
}

void FrameBuffer::closeColorBuffer(HandleType p_colorbuffer)
{
    android::Mutex::Autolock mutex(m_lock);
    ColorBufferMap::iterator c(m_colorbuffers.find(p_colorbuffer));
    if (c == m_colorbuffers.end()) {
        // bad colorbuffer handle
        return;
    }
    if (--(*c).second.refcount == 0) {
        m_colorbuffers.erase(c);
    }
}

bool FrameBuffer::flushWindowSurfaceColorBuffer(HandleType p_surface)
{
    android::Mutex::Autolock mutex(m_lock);

    WindowSurfaceMap::iterator w( m_windows.find(p_surface) );
    if (w == m_windows.end()) {
        // bad surface handle
        return false;
    }

    (*w).second->flushColorBuffer();

    return true;
}

bool FrameBuffer::setWindowSurfaceColorBuffer(HandleType p_surface,
                                              HandleType p_colorbuffer)
{
    android::Mutex::Autolock mutex(m_lock);

    WindowSurfaceMap::iterator w( m_windows.find(p_surface) );
    if (w == m_windows.end()) {
        // bad surface handle
        return false;
    }

    ColorBufferMap::iterator c( m_colorbuffers.find(p_colorbuffer) );
    if (c == m_colorbuffers.end()) {
        // bad colorbuffer handle
        return false;
    }

    (*w).second->setColorBuffer( (*c).second.cb );

    return true;
}

bool FrameBuffer::updateColorBuffer(HandleType p_colorbuffer,
                                    int x, int y, int width, int height,
                                    GLenum format, GLenum type, void *pixels)
{
    android::Mutex::Autolock mutex(m_lock);

    ColorBufferMap::iterator c( m_colorbuffers.find(p_colorbuffer) );
    if (c == m_colorbuffers.end()) {
        // bad colorbuffer handle
        return false;
    }

    (*c).second.cb->subUpdate(x, y, width, height, format, type, pixels);

    return true;
}

bool FrameBuffer::bindColorBufferToTexture(HandleType p_colorbuffer)
{
    android::Mutex::Autolock mutex(m_lock);

    ColorBufferMap::iterator c( m_colorbuffers.find(p_colorbuffer) );
    if (c == m_colorbuffers.end()) {
        // bad colorbuffer handle
        return false;
    }

    return (*c).second.cb->bindToTexture();
}

bool FrameBuffer::bindColorBufferToRenderbuffer(HandleType p_colorbuffer)
{
    android::Mutex::Autolock mutex(m_lock);

    ColorBufferMap::iterator c( m_colorbuffers.find(p_colorbuffer) );
    if (c == m_colorbuffers.end()) {
        // bad colorbuffer handle
        return false;
    }

    return (*c).second.cb->bindToRenderbuffer();
}

bool FrameBuffer::bindContext(HandleType p_context,
                              HandleType p_drawSurface,
                              HandleType p_readSurface)
{
    android::Mutex::Autolock mutex(m_lock);

    WindowSurfacePtr draw(NULL), read(NULL);
    RenderContextPtr ctx(NULL);

    //
    // if this is not an unbind operation - make sure all handles are good
    //
    if (p_context || p_drawSurface || p_readSurface) {
        RenderContextMap::iterator r( m_contexts.find(p_context) );
        if (r == m_contexts.end()) {
            // bad context handle
            return false;
        }

        ctx = (*r).second;
        WindowSurfaceMap::iterator w( m_windows.find(p_drawSurface) );
        if (w == m_windows.end()) {
            // bad surface handle
            return false;
        }
        draw = (*w).second;

        if (p_readSurface != p_drawSurface) {
            WindowSurfaceMap::iterator w( m_windows.find(p_readSurface) );
            if (w == m_windows.end()) {
                // bad surface handle
                return false;
            }
            read = (*w).second;
        }
        else {
            read = draw;
        }
    }

    if (!s_egl.eglMakeCurrent(m_eglDisplay,
                              draw ? draw->getEGLSurface() : EGL_NO_SURFACE,
                              read ? read->getEGLSurface() : EGL_NO_SURFACE,
                              ctx ? ctx->getEGLContext() : EGL_NO_CONTEXT)) {
        // MakeCurrent failed
        ERR("eglMakeCurrent failed\n");
        return false;
    }

    //
    // Bind the surface(s) to the context
    //
    RenderThreadInfo *tinfo = RenderThreadInfo::get();
    WindowSurfacePtr bindDraw, bindRead;
    if (draw.Ptr() == NULL && read.Ptr() == NULL) {
        // Unbind the current read and draw surfaces from the context
        bindDraw = tinfo->currDrawSurf;
        bindRead = tinfo->currReadSurf;
    } else {
        bindDraw = draw;
        bindRead = read;
    }

    if (bindDraw.Ptr() != NULL && bindRead.Ptr() != NULL) {
        if (bindDraw.Ptr() != bindRead.Ptr()) {
            bindDraw->bind(ctx, SURFACE_BIND_DRAW);
            bindRead->bind(ctx, SURFACE_BIND_READ);
        }
        else {
            bindDraw->bind( ctx, SURFACE_BIND_READDRAW );
        }
    }

    //
    // update thread info with current bound context
    //
    tinfo->currContext = ctx;
    tinfo->currDrawSurf = draw;
    tinfo->currReadSurf = read;
    if (ctx) {
        if (ctx->isGL2()) tinfo->m_gl2Dec.setContextData(&ctx->decoderContextData());
        else tinfo->m_glDec.setContextData(&ctx->decoderContextData());
    }
    else {
        tinfo->m_glDec.setContextData(NULL);
        tinfo->m_gl2Dec.setContextData(NULL);
    }
    return true;
}

//
// The framebuffer lock should be held when calling this function !
//
bool FrameBuffer::bind_locked()
{
    EGLContext prevContext = s_egl.eglGetCurrentContext();
    EGLSurface prevReadSurf = s_egl.eglGetCurrentSurface(EGL_READ);
    EGLSurface prevDrawSurf = s_egl.eglGetCurrentSurface(EGL_DRAW);

    if (!s_egl.eglMakeCurrent(m_eglDisplay, m_pbufSurface,
                              m_pbufSurface, m_pbufContext)) {
        ERR("eglMakeCurrent failed\n");
        return false;
    }

    m_prevContext = prevContext;
    m_prevReadSurf = prevReadSurf;
    m_prevDrawSurf = prevDrawSurf;
    return true;
}

bool FrameBuffer::bindSubwin_locked()
{
    EGLContext prevContext = s_egl.eglGetCurrentContext();
    EGLSurface prevReadSurf = s_egl.eglGetCurrentSurface(EGL_READ);
    EGLSurface prevDrawSurf = s_egl.eglGetCurrentSurface(EGL_DRAW);

    if (!s_egl.eglMakeCurrent(m_eglDisplay, m_eglSurface,
                              m_eglSurface, m_eglContext)) {
        ERR("eglMakeCurrent failed\n");
        return false;
    }

    //
    // initialize GL state in eglContext if not yet initilaized
    //
    if (!m_eglContextInitialized) {
        initGLState(m_width, m_height);
        m_eglContextInitialized = true;
    }

    m_prevContext = prevContext;
    m_prevReadSurf = prevReadSurf;
    m_prevDrawSurf = prevDrawSurf;
    return true;
}

bool FrameBuffer::unbind_locked()
{
    if (!s_egl.eglMakeCurrent(m_eglDisplay, m_prevDrawSurf,
                              m_prevReadSurf, m_prevContext)) {
        return false;
    }

    m_prevContext = EGL_NO_CONTEXT;
    m_prevReadSurf = EGL_NO_SURFACE;
    m_prevDrawSurf = EGL_NO_SURFACE;
    return true;
}

bool FrameBuffer::post(HandleType p_colorbuffer, bool needLock)
{
    if (needLock) m_lock.lock();
    bool ret = false;

    ColorBufferMap::iterator c( m_colorbuffers.find(p_colorbuffer) );
    if (c != m_colorbuffers.end()) {

        m_lastPostedColorBuffer = p_colorbuffer;
        if (!m_subWin) {
            // no subwindow created for the FB output
            // cannot post the colorbuffer
            if (needLock) m_lock.unlock();
            return ret;
        }

        // bind the subwindow eglSurface
        if (!bindSubwin_locked()) {
            fprintf(stderr, "FrameBuffer::post eglMakeCurrent failed\n");
            if (needLock) m_lock.unlock();
            return false;
        }

        //
        // Send framebuffer to callback
        //
        if (m_onPost) {
            s_gl.glMatrixMode(GL_PROJECTION);
            s_gl.glPushMatrix();
            initGLState(m_width, m_height);
            s_gl.glBindFramebufferOES(GL_FRAMEBUFFER_OES, m_framebuffer);
            s_gl.glViewport(0, 0, m_width, m_height);

            ret = (*c).second.cb->post();
            if(m_textLogo)
                displayLogo();

            if (ret) {
                s_gl.glReadPixels(0, 0, m_width, m_height,
                                  GL_BGRA_EXT, GL_UNSIGNED_BYTE, m_fbImage);
                m_onPost(m_onPostContext, m_width, m_height, -1,
                         GL_BGRA_EXT, GL_UNSIGNED_BYTE, m_fbImage);
            }
            s_gl.glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
            s_gl.glViewport(0, 0, m_FBwidth, m_FBheight);
            s_gl.glMatrixMode(GL_PROJECTION);
            s_gl.glPopMatrix();
            s_gl.glMatrixMode(GL_MODELVIEW);
        }

        //
        // render the color buffer to the window
        //
        s_gl.glPushMatrix();
        s_gl.glTranslatef(m_x, m_y, 0.0f);
        s_gl.glRotatef(m_zRot, 0.0f, 0.0f, 1.0f);
        s_gl.glClear(GL_COLOR_BUFFER_BIT);

        ret = (*c).second.cb->post();

        s_gl.glRotatef(-m_zRot, 0.0f, 0.0f, 1.0f);

        if(m_textLogo) {
            displayLogo();
        }

        s_gl.glPopMatrix();

        if(m_windowHighlight) {
            s_gl.glMatrixMode(GL_PROJECTION);
            s_gl.glPushMatrix();
            initGLState(1, 1);
            displayWindowHighlight();
            s_gl.glMatrixMode(GL_PROJECTION);
            s_gl.glPopMatrix();
            s_gl.glMatrixMode(GL_MODELVIEW);
        }

        if (ret) {
            s_egl.eglSwapBuffers(m_eglDisplay, m_eglSurface);
        }

        // restore previous binding
        unbind_locked();
    }

    if (needLock) m_lock.unlock();
    return ret;
}

bool FrameBuffer::repost()
{
    if (m_lastPostedColorBuffer) {
        return post( m_lastPostedColorBuffer );
    }
    return false;
}

void FrameBuffer::initGLState(float w, float h)
{
    s_gl.glMatrixMode(GL_PROJECTION);
    s_gl.glLoadIdentity();
    s_gl.glOrthof(-w/2.0f, w/2.0f, -h/2.0f, h/2.0f, -1.0, 1.0);
    s_gl.glMatrixMode(GL_MODELVIEW);
    s_gl.glLoadIdentity();
    s_gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void FrameBuffer::setViewport(float width, float height)
{
    if (s_theFrameBuffer) {

        s_theFrameBuffer->m_lock.lock();

        if (s_theFrameBuffer->bindSubwin_locked()) {
            s_theFrameBuffer->initGLState(width, height);
            s_theFrameBuffer->unbind_locked();
        }

        s_theFrameBuffer->m_lock.unlock();
    }
}

void FrameBuffer::scrollViewport(float x, float y)
{
    if (s_theFrameBuffer) {
        s_theFrameBuffer->m_lock.lock();
        s_theFrameBuffer->m_x = x;
        s_theFrameBuffer->m_y = y;
        s_theFrameBuffer->m_lock.unlock();
    }
}

bool FrameBuffer::registerOGLCallback(OnPostFn onPost, void* onPostContext)
{
    bool success = true;

    if (s_theFrameBuffer) {

        s_theFrameBuffer->m_lock.lock();
        s_theFrameBuffer->bindSubwin_locked();

        s_theFrameBuffer->m_onPost = onPost;
        s_theFrameBuffer->m_onPostContext = onPostContext;

        if(onPost != NULL && s_theFrameBuffer->m_fbImage == NULL) {
            s_theFrameBuffer->m_fbImage = (unsigned char*)malloc(4 * s_theFrameBuffer->m_width * s_theFrameBuffer->m_height);
            if (!s_theFrameBuffer->m_fbImage) {
                ERR("Failed to allocate space for onPost framebuffer image\n");
                success = false;
            }
            // Create framebuffer of fixed size
            GLuint renderbuffer;
            s_gl.glGenRenderbuffersOES(1, &renderbuffer);
            s_gl.glBindRenderbufferOES(GL_RENDERBUFFER_OES, renderbuffer);
            s_gl.glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_RGBA8_OES,
                                          s_theFrameBuffer->m_width, s_theFrameBuffer->m_height);

            s_gl.glGenFramebuffersOES(1, &s_theFrameBuffer->m_framebuffer);
            s_gl.glBindFramebufferOES(GL_FRAMEBUFFER_OES, s_theFrameBuffer->m_framebuffer);
            s_gl.glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES,
                                              GL_RENDERBUFFER_OES, renderbuffer);
            s_gl.glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
        }
        s_theFrameBuffer->unbind_locked();
        s_theFrameBuffer->m_lock.unlock();
    }

    return success;
}

bool FrameBuffer::playScreenshotAnimation(void)
{
    if (s_theFrameBuffer) {
            // Let's play a nice visual effect.
        s_theFrameBuffer->cameraEffect(250); // ms
        // Need to refresh the screen after the visual effect
        s_theFrameBuffer->repost();
    }
}

void FrameBuffer::setTexture(char* data, int width, int height, GLuint* texture)
{
    if(texture) {
        s_gl.glDeleteTextures(1, texture);
        *texture = 0;
    }

    if(data) {
        s_gl.glGenTextures(1, texture);
        s_gl.glBindTexture(GL_TEXTURE_2D, *texture);
        s_gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                          0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        s_gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        s_gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        s_gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        s_gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        s_gl.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    }
}

void FrameBuffer::displayTexture(GLuint text, int x0, int y0, int width, int height)
{
    GLfloat verts[12];

    verts[0] = x0;
    verts[1] = y0;
    verts[2] = 0.0f;
    verts[3] = verts[0];
    verts[4] = verts[1]+height;
    verts[5] = 0.0f;
    verts[6] = verts[0]+width;
    verts[7] = verts[1];
    verts[8] = 0.0f;
    verts[9] = verts[0]+width;
    verts[10] = verts[1]+height;
    verts[11] = 0.0f;

    GLfloat tcoords[] = { 0.0f, 1.0f,
                          0.0f, 0.0f,
                          1.0f, 1.0f,
                          1.0f, 0.0f };

    s_gl.glEnable(GL_BLEND);
    s_gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    s_gl.glBindTexture(GL_TEXTURE_2D, text);
    s_gl.glEnable(GL_TEXTURE_2D);
    s_gl.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    s_gl.glClientActiveTexture(GL_TEXTURE0);
    s_gl.glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    s_gl.glTexCoordPointer(2, GL_FLOAT, 0, tcoords);

    s_gl.glEnableClientState(GL_VERTEX_ARRAY);
    s_gl.glVertexPointer(3, GL_FLOAT, 0, verts);
    s_gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    s_gl.glDisable(GL_BLEND);
}

void FrameBuffer::displayLogo()
{
    if(m_textLogo) {
        int diag = m_width + m_height;
        int w = (int)(diag/8.0);
        int h = (int)(w/m_logoRatio);
        int pad = 8;

        if(m_zRot == 90.0f || m_zRot == 270.0f) {
            displayTexture(m_textLogo, -m_height/2.0f+pad, -m_width/2.0f+pad, w, h);
        }
        else {
            displayTexture(m_textLogo, -m_width/2.0f+pad, -m_height/2.0f+pad, w, h);
        }
    }
}

void FrameBuffer::displayStartScreen()
{
    displayTexture(m_textStartScreeen, 0, 0, m_FBwidth, m_FBheight);
}

void FrameBuffer::setLogo(char* logo, int width, int height)
{
    if (s_theFrameBuffer) {
        if(height != 0) {
            s_theFrameBuffer->m_logoRatio = width/height;
        }
        else {
            s_theFrameBuffer->m_logoRatio = 0.0;
        }
        s_theFrameBuffer->m_lock.lock();
        s_theFrameBuffer->bind_locked();
        setTexture(logo, width, height, &s_theFrameBuffer->m_textLogo);
        s_theFrameBuffer->unbind_locked();
        s_theFrameBuffer->m_lock.unlock();
    }
}

void FrameBuffer::setStartScreen(char* image, int width, int height)
{
    if (s_theFrameBuffer) {
        s_theFrameBuffer->m_lock.lock();
        s_theFrameBuffer->bind_locked();
        setTexture(image, width, height, &s_theFrameBuffer->m_textStartScreeen);
        s_theFrameBuffer->unbind_locked();
        s_theFrameBuffer->m_lock.unlock();
    }
}

void FrameBuffer::setWindowHighlight(bool value)
{
    if (s_theFrameBuffer) {
        if (s_theFrameBuffer->m_windowHighlight != value)
            s_theFrameBuffer->m_windowHighlight = value;
    }
}

void FrameBuffer::displayWindowHighlight()
{
    GLfloat verts[] = { +0.495f, -0.495f, 0.0f,
                        +0.495f, +0.495f, 0.0f,
                        -0.495f, +0.495f, 0.0f,
                        -0.495f, -0.495f, 0.0f,
                        +0.495f, -0.495f, 0.0f };

    s_gl.glDisable(GL_TEXTURE_2D);

    /*
    s_gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    s_gl.glEnable(GL_BLEND);
    */

    s_gl.glEnable(GL_LINE_SMOOTH);
    s_gl.glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    s_gl.glEnableClientState(GL_VERTEX_ARRAY);
    s_gl.glVertexPointer(3, GL_FLOAT, 0, verts);
    s_gl.glLineWidth(3);
    s_gl.glColor4f( 230.f/255.f, 25.f/255.f, 94.f/255.f, 255.f/255.f );
    s_gl.glDrawArrays(GL_LINE_STRIP, 0, 5);
    s_gl.glLineWidth(1);
    s_gl.glDisableClientState(GL_VERTEX_ARRAY);
}

void FrameBuffer::cameraEffect(int duration)
{
    GLuint originalTex;
    GLuint greyTex;

    unsigned char* greyImg = (unsigned char*)malloc(4*m_width*m_height);

    if(greyImg == NULL) {
        // malloc failed, just return without visual effect
        return;
    }

    // FrameBuffer capture is made in BGRA format
    // glTexImage2D wants RGBA
    for(int i=0; i<(m_width*m_height); i++) {
        // Swap Red <-> Blue channels
        unsigned char tmp = m_fbImage[4*i+2];
        m_fbImage[4*i+2] = m_fbImage[4*i];
        m_fbImage[4*i] = tmp;

        // Greyscale conversion
        greyImg[4*i] = (unsigned char)(m_fbImage[4*i]*0.299 + m_fbImage[4*i+1]*0.587 + m_fbImage[4*i+2]*0.114);
        greyImg[4*i+1] = greyImg[4*i];
        greyImg[4*i+2] = greyImg[4*i];
        greyImg[4*i+3] = 255;
    }

    s_theFrameBuffer->m_lock.lock();
    s_theFrameBuffer->bind_locked();
    setTexture((char*)m_fbImage, m_width, m_height, &originalTex);
    setTexture((char*)greyImg, m_width, m_height, &greyTex);
    s_theFrameBuffer->unbind_locked();
    s_theFrameBuffer->m_lock.unlock();

    long long start = GetCurrentTimeMS();
    long long elapsed = 0;

    do{
        s_theFrameBuffer->m_lock.lock();
        s_theFrameBuffer->bindSubwin_locked();

        s_gl.glPushMatrix();
        // translation (scrolling)
        s_gl.glTranslatef(m_x, m_y, 0.0f);
        // rotation according to VM orientation
        s_gl.glRotatef(m_zRot, 0.0f, 0.0f, 1.0f);
        // Vertical flip
        s_gl.glScalef(1, -1, 1);

        // display captured color frambuffer in background
        displayTexture(originalTex, -m_width/2.0f, -m_height/2.0, m_width, m_height); 
        s_gl.glPopMatrix();

        // shrinking factor non-linear
        float factor = 1.0 - 0.98*elapsed*elapsed/duration/duration;
        int w = m_width*factor;
        int h = m_height*factor;

        // Center shrinked captured image
        s_gl.glPushMatrix();
        // rotation according to VM orientation
        s_gl.glRotatef(m_zRot, 0.0f, 0.0f, 1.0f);
        // Vertical flip
        s_gl.glScalef(1, -1, 1);
        // display grayscale shrinked captured image
        displayTexture(greyTex, -w/2.0f, -h/2.0f, w, h);
        s_gl.glPopMatrix();

        s_egl.eglSwapBuffers(m_eglDisplay, m_eglSurface);
        elapsed = GetCurrentTimeMS() - start;

        s_theFrameBuffer->unbind_locked();
        s_theFrameBuffer->m_lock.unlock();

    } while(elapsed <= duration);

    // free textures
    s_theFrameBuffer->m_lock.lock();
    s_theFrameBuffer->bind_locked();
    setTexture(NULL, 0, 0, &originalTex);
    setTexture(NULL, 0, 0, &greyTex);
    s_theFrameBuffer->unbind_locked();
    s_theFrameBuffer->m_lock.unlock();

    free(greyImg);
}

