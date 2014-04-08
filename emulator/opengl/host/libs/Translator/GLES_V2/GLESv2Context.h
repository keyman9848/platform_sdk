/*
* Copyright 2011 The Android Open Source Project
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

#ifndef GLES_V2_CONTEXT_H
#define GLES_V2_CONTEXT_H

#include <GLcommon/GLDispatch.h>
#include <GLcommon/GLEScontext.h>
#include <GLcommon/objectNameManager.h>
#include <utils/threads.h>
#include <list>


class GLESv2Context : public GLEScontext{
public:
    void init();
    GLESv2Context();
    virtual ~GLESv2Context();
    void setupArraysPointers(GLESConversionArrays& fArrs,GLint first,GLsizei count,GLenum type,const GLvoid* indices,bool direct);
    int  getMaxTexUnits();

    // This whole att0 thing is about a incompatibility between GLES and OpenGL.
    // GLES allows a vertex shader attribute to be in location 0 and have a
    // current value, while OpenGL is not very clear about this, which results
    // in each implementation doing something different.
    void setAttribute0value(float x, float y, float z, float w);
    void validateAtt0PreDraw(unsigned int count);
    void validateAtt0PostDraw(void);
    const float* getAtt0(void) {return m_attribute0value;}
    void addProgram(GLuint program) {m_programs.push_back(program);}
    void removeProgram(GLuint program) {m_programs.remove(program);}
    void addShader(GLuint shader) {m_shaders.push_back(shader);}
    void removeShader(GLuint shader) {m_shaders.remove(shader);}
    void addTexture(GLuint texture) {m_textures.push_back(texture);}
    void removeTexture(GLuint texture) {m_textures.remove(texture);}
    void addBuffer(GLuint buffer) {m_buffers.push_back(buffer);}
    void removeBuffer(GLuint buffer) {m_buffers.remove(buffer);}
    void addFramebuffer(GLuint framebuffers) {m_framebuffers.push_back(framebuffers);}
    void removeFramebuffer(GLuint framebuffers) {m_framebuffers.remove(framebuffers);}
    void addRenderbuffer(GLuint renderbuffers) {m_renderbuffers.push_back(renderbuffers);}
    void removeRenderbuffer(GLuint renderbuffers) {m_renderbuffers.remove(renderbuffers);}

protected:
    bool needConvert(GLESConversionArrays& fArrs,GLint first,GLsizei count,GLenum type,const GLvoid* indices,bool direct,GLESpointer* p,GLenum array_id);
private:
    void setupArr(const GLvoid* arr,GLenum arrayType,GLenum dataType,GLint size,GLsizei stride,GLboolean normalized, int pointsIndex = -1);
    void initExtensionString();

    float m_attribute0value[4];
    GLfloat* m_att0Array;
    unsigned int m_att0ArrayLength;
    bool m_att0NeedsDisable;

    std::list<GLuint> m_programs;
    std::list<GLuint> m_shaders;
    std::list<GLuint> m_textures;
    std::list<GLuint> m_buffers;
    std::list<GLuint> m_framebuffers;
    std::list<GLuint> m_renderbuffers;
};

#endif
