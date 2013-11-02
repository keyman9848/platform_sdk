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
#include "EglContext.h"
#include "EglDisplay.h"
#include "EglGlobalInfo.h"
#include "EglOsApi.h"

#include <stdio.h>

#include "egl_internal.h"

unsigned int EglContext::s_nextContextHndl = 0;

extern EglGlobalInfo* g_eglInfo; // defined in EglImp.cpp

static EGLSurface g_fakeSurface = EGL_NO_SURFACE;

static void doDestroyObject(int type, int id, int ver) {
    fprintf(stderr, "doDestroyObject: type=%d id=%d ver=%d\n", type, id, ver);
    fprintf(stderr, "doDestroyObject: destroyObject=%p\n", g_eglInfo->getIface((GLESVersion)ver)->destroyObject);
    g_eglInfo->getIface((GLESVersion)ver)->destroyObject(type, id);
}

bool EglContext::usingSurface(SurfacePtr surface) {
  return surface.Ptr() == m_read.Ptr() || surface.Ptr() == m_draw.Ptr();
}

EglContext::EglContext(EglDisplay *dpy, EGLNativeContextType context,ContextPtr shared_context,
            EglConfig* config,GLEScontext* glesCtx,GLESVersion ver,ObjectNameManager* mngr):
m_dpy(dpy),
m_native(context),
m_config(config),
m_glesContext(glesCtx),
m_read(NULL),
m_draw(NULL),
m_version(ver),
m_mngr(mngr)
{
    m_shareGroup = shared_context.Ptr()?
                   mngr->attachShareGroup(context,shared_context->nativeType()):
                   mngr->createShareGroup(context);
    m_hndl = ++s_nextContextHndl;
}

EglContext::~EglContext()
{
    fprintf(stderr, "[PID %d] destruct EglContext %p SG %p\n", 999, this, m_shareGroup.Ptr());

    EGLContext prevContext = internal_eglGetCurrentContext();
    EGLSurface prevReadSurf = internal_eglGetCurrentSurface(EGL_READ);
    EGLSurface prevDrawSurf = internal_eglGetCurrentSurface(EGL_DRAW);
    fprintf(stderr, "current Context = %d read = %d draw = %d\n", prevContext, prevReadSurf, prevDrawSurf);
    fprintf(stderr, "own Context = %d read = %d draw = %d\n", m_hndl, m_read.Ptr(), m_draw.Ptr());
    if (g_fakeSurface == EGL_NO_SURFACE) {
        GLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };

        GLint configAttribs2[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };

        EGLConfig fconfig;
        int n;
        int retcfg;
        if (m_version == GLES_2_0) {
            fprintf(stderr, "GLES is 2.0\n");
            retcfg = internal_eglChooseConfig((EGLDisplay)m_dpy, configAttribs2, &fconfig, 1, &n);
        }
        else {
            fprintf(stderr, "GLES is 1.1\n");
            retcfg = internal_eglChooseConfig((EGLDisplay)m_dpy, configAttribs, &fconfig, 1, &n);
        }
        if (!retcfg) {
            fprintf(stderr, "Unable to choose config\n");
        }

        EGLint pbufAttribs[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };

        g_fakeSurface = internal_eglCreatePbufferSurface((EGLDisplay)m_dpy, fconfig, pbufAttribs);
        if (g_fakeSurface == EGL_NO_SURFACE)
            fprintf(stderr, "Unable to create fake surface\n");
    }

    SurfacePtr surfPtr = m_dpy->getSurface(g_fakeSurface);
    if (!surfPtr.Ptr())
        fprintf(stderr, "Unable to get fake surface\n");
    else {
        if (EglOS::makeCurrent(m_dpy->nativeType(), surfPtr.Ptr(), surfPtr.Ptr(), nativeType()) == EGL_FALSE)
            fprintf(stderr, "Unable to switch to context, eglGetError=%X\n", internal_eglGetError());
    }

    if (m_mngr)
    {
        m_mngr->destroyShareGroup(m_native, m_version, doDestroyObject);
        m_mngr->deleteShareGroup(m_native);
    }

    // Switch back to previous context
    if (internal_eglMakeCurrent((EGLDisplay)m_dpy, prevReadSurf, prevDrawSurf, prevContext) == EGL_FALSE) {
        fprintf(stderr, "Unable to switch to previous context\n");
    }

    //
    // remove the context in the underlying OS layer
    // 
    EglOS::destroyContext(m_dpy->nativeType(),m_native);

    //
    // call the client-api to remove the GLES context
    // 
    g_eglInfo->getIface(version())->deleteGLESContext(m_glesContext);
}

void EglContext::setSurfaces(SurfacePtr read,SurfacePtr draw)
{
    m_read = read;
    m_draw = draw;
}

bool EglContext::getAttrib(EGLint attrib,EGLint* value) {
    switch(attrib) {
    case EGL_CONFIG_ID:
        *value = m_config->id();
        break;
    default:
        return false;
    }
    return true;
}

bool EglContext::attachImage(unsigned int imageId,ImagePtr img){
   if(m_attachedImages.find(imageId) == m_attachedImages.end()){
       m_attachedImages[imageId] = img;
       return true;
   }
   return false;
}

void EglContext::detachImage(unsigned int imageId){
    m_attachedImages.erase(imageId);
}

