#include <EGL/egl.h>
#include "egl_internal.h"

EGLint EGLAPIENTRY internal_eglGetError(void) {
    return eglGetError();
}

EGLBoolean EGLAPIENTRY internal_eglChooseConfig(EGLDisplay display, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config) {
    return eglChooseConfig(display, attrib_list, configs, config_size, num_config);
}

EGLSurface EGLAPIENTRY internal_eglCreatePbufferSurface(EGLDisplay display, EGLConfig config, const EGLint *attrib_list) {
    return eglCreatePbufferSurface(display, config, attrib_list);
}

EGLBoolean EGLAPIENTRY internal_eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context) {
    return eglMakeCurrent(display, draw, read, context);
}

EGLContext EGLAPIENTRY internal_eglGetCurrentContext(void) {
    return eglGetCurrentContext();
}

EGLSurface EGLAPIENTRY internal_eglGetCurrentSurface(EGLint readdraw) {
    return eglGetCurrentSurface(readdraw);
}
