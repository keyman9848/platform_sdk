#include "FrameBuffer.h"
#include "RenderServer.h"

extern "C" {
    int AndroVM_initLibrary();
    int AndroVM_FrameBuffer_initialize(int w, int h);
    int AndroVM_FrameBuffer_setupSubWindow(void *id, int x, int y, int w, int h, float zrot);
    int AndroVM_FrameBuffer_removeSubWindow();
    void *AndroVM_FrameBuffer_getSubWindow();
    int AndroVM_RenderServer_create(int p);
    int AndroVM_RenderServer_Main();
    int AndroVM_RenderServer_start();
    int AndroVM_setStreamMode(int);
    int AndroVM_setVMIP(char *);
    void AndroVM_setOpenGLDisplayRotation(float);
    bool AndroVM_initOpenGLRenderer(int, int, int, OnPostFn, void*);
    void AndroVM_setCallbackRotation(void (* fn)(float));
    void AndroVM_repaintOpenGLDisplay();
    void AndroVM_setDPI(int);
    void AndroVM_setViewport(int width, int height);
    void AndroVM_scrollViewport(int x, int y);
    float AndroVM_getDisplayRotation();
    bool AndroVM_registerOGLCallback(OnPostFn, void*);
    void AndroVM_setLogo(char* logo, int width, int height);
    void AndroVM_setStartScreen(char* img, int width, int height);
    void AndroVM_setWindowHighlight(bool value);
    void AndroVM_playScreenshotAnimation(void);
    void AndroVM_shutdown(void);
}

int AndroVM_initLibrary()
{
    return initLibrary();
}

int AndroVM_FrameBuffer_initialize(int w, int h)
{
    return FrameBuffer::initialize(w, h, NULL, NULL);
}

int AndroVM_FrameBuffer_setupSubWindow(void *id, int x, int y, int w, int h, float zrot)
{
    return FrameBuffer::setupSubWindow((FBNativeWindowType)id, x, y, w, h, zrot);
}

int AndroVM_FrameBuffer_removeSubWindow() {
    return FrameBuffer::removeSubWindow();
}

void *AndroVM_FrameBuffer_getSubWindow() {
    FrameBuffer *fb = FrameBuffer::getFB();

    if (!fb)
        return NULL;

    return (void *)fb->getSubWindow();
}

static RenderServer *l_rserver = NULL;

int AndroVM_RenderServer_create(int p)
{
    l_rserver = RenderServer::create(p);
    return (l_rserver != NULL);
}

int AndroVM_RenderServer_Main()
{
    if (l_rserver)
        return l_rserver->Main();
    else
        return -1;
}

int AndroVM_RenderServer_start()
{
    if (l_rserver)
        return l_rserver->start();
    else
        return -1;
}

int AndroVM_setStreamMode(int m)
{
    return setStreamMode(m);
}

int AndroVM_setVMIP(char *ip)
{
    return setVMIP(ip);
}

void AndroVM_setOpenGLDisplayRotation(float zRot)
{
    setOpenGLDisplayRotation(zRot);
}

bool AndroVM_initOpenGLRenderer(int width, int height, int portNum, OnPostFn onPost, void* onPostContext)
{
    return initOpenGLRenderer(width, height, portNum, onPost, onPostContext);
}

bool AndroVM_registerOGLCallback(OnPostFn onPost, void* onPostContext)
{
    return FrameBuffer::registerOGLCallback(onPost, onPostContext);
}

void AndroVM_setCallbackRotation(void (* fn)(float)) {
    setCallbackRotation(fn);
}

void AndroVM_repaintOpenGLDisplay() {
    repaintOpenGLDisplay();
}

void AndroVM_setDPI(int d) {
    setDPI(d);
}

void AndroVM_setViewport(int width, int height)
{
    FrameBuffer::setViewport(width, height);
}

void AndroVM_scrollViewport(int x, int y)
{
    FrameBuffer::scrollViewport(x, y);
}

void AndroVM_setLogo(char* logo, int width, int height)
{
    FrameBuffer::setLogo(logo, width, height);
}

void AndroVM_setStartScreen(char* logo, int width, int height)
{
    FrameBuffer::setStartScreen(logo, width, height);
}

void AndroVM_setWindowHighlight(bool value)
{
    FrameBuffer::setWindowHighlight(value);
}

void AndroVM_playScreenshotAnimation(void)
{
    FrameBuffer::playScreenshotAnimation();
}

int hasToStop = 0;

void AndroVM_shutdown(void)
{
    hasToStop = 1;

    FrameBuffer::finalize();
}
