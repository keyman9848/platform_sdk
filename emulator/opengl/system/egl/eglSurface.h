#ifndef _SYSTEM_EGL_SURFACE_H
#define _SYSTEM_EGL_SURFACE_H

#include <EGL/egl.h>

//egl_surface_t

//we don't need to handle depth since it's handled when window created on the host

struct egl_surface_t {

    EGLDisplay          dpy;
    EGLConfig           config;


    egl_surface_t(EGLDisplay dpy, EGLConfig config, EGLint surfaceType);
    virtual     ~egl_surface_t();

    virtual     void        setSwapInterval(int interval) = 0;
    virtual     EGLBoolean  swapBuffers() = 0;

    EGLint      getSwapBehavior() const;
    uint32_t    getRcSurface()   { return rcSurface; }
    EGLint      getSurfaceType() { return surfaceType; }

    EGLint      getWidth(){ return width; }
    EGLint      getHeight(){ return height; }
    void        setTextureFormat(EGLint _texFormat) { texFormat = _texFormat; }
    EGLint      getTextureFormat() { return texFormat; }
    void        setTextureTarget(EGLint _texTarget) { texTarget = _texTarget; }
    EGLint      getTextureTarget() { return texTarget; }

private:
    //
    //Surface attributes
    //
    EGLint      width;
    EGLint      height;
    EGLint      texFormat;
    EGLint      texTarget;

protected:
    void        setWidth(EGLint w)  { width = w;  }
    void        setHeight(EGLint h) { height = h; }

    EGLint      surfaceType;
    uint32_t    rcSurface; //handle to surface created via remote control
};

#endif
