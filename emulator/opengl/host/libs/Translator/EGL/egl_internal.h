extern "C" {

    EGLContext EGLAPIENTRY internal_eglGetCurrentContext(void);
    EGLSurface EGLAPIENTRY internal_eglGetCurrentSurface(EGLint readdraw);
    EGLBoolean EGLAPIENTRY internal_eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context);
    EGLint EGLAPIENTRY internal_eglGetError(void);
    EGLBoolean EGLAPIENTRY internal_eglChooseConfig(EGLDisplay display, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
    EGLSurface EGLAPIENTRY internal_eglCreatePbufferSurface(EGLDisplay display, EGLConfig config, const EGLint *attrib_list);

}
