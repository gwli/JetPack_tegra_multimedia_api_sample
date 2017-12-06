/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "NvEglRenderer.h"
#include "NvLogging.h"
#include "nvbuf_utils.h"

#include <GLES3/gl32.h>

#include <cstring>
#include <sys/time.h>
#include <assert.h>

#include "nvToolsExt.h"
#include "v4l2_backend_test.h"

#define CAT_NAME "EglRenderer"

#define ERROR_GOTO_FAIL(val, string) \
    do { \
        if (val) {\
            CAT_DEBUG_MSG(string); \
            goto fail; \
        } \
    } while (0)

PFNEGLCREATEIMAGEKHRPROC                NvEglRenderer::eglCreateImageKHR;
PFNEGLDESTROYIMAGEKHRPROC               NvEglRenderer::eglDestroyImageKHR;
PFNEGLCREATESYNCKHRPROC                 NvEglRenderer::eglCreateSyncKHR;
PFNEGLDESTROYSYNCKHRPROC                NvEglRenderer::eglDestroySyncKHR;
PFNEGLCLIENTWAITSYNCKHRPROC             NvEglRenderer::eglClientWaitSyncKHR;
PFNEGLGETSYNCATTRIBKHRPROC              NvEglRenderer::eglGetSyncAttribKHR;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC     NvEglRenderer::glEGLImageTargetTexture2DOES;

using namespace std;

static const float kVertices[] = { -1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, };
static const float kTextureCoords[] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, };

// Vertex for rect
static const float kF = 0.15;
static const float kH = 0.01;
static const float kVerticesRect[] = { 0.f - kF, 0.f + kF, 0.f - kF, 0.f - kF
        - 0.05f, 0.f - kH, 0.f - kF - 0.05f, 0.f - kH, 0.f + kH, 0.f + kF
        + 0.05f, 0.f + kH, 0.f + kF + 0.05f, 0.f + kF };

NvEglRenderer::NvEglRenderer(const char *name, uint32_t width, uint32_t height,
        uint32_t x_offset, uint32_t y_offset, bool threaded)
        :NvElement(name, valid_fields)
{
    int depth;
    int screen_num;
    XSetWindowAttributes window_attributes;
    x_window = 0;
    x_display = NULL;

    texture_id = 0;
    gc = NULL;
    fontinfo = NULL;

    egl_surface = EGL_NO_SURFACE;
    egl_context = EGL_NO_CONTEXT;
    egl_display = EGL_NO_DISPLAY;
    egl_config = NULL;

    memset(&last_render_time, 0, sizeof(last_render_time));
    is_threaded = threaded;
    stop_thread = false;
    render_thread = 0;

    memset(overlay_str, 0, sizeof(overlay_str));
    overlay_str_x_offset = 0;
    overlay_str_y_offset = 0;

    if(is_threaded)
    {
		pthread_mutex_init(&render_lock, NULL);
		pthread_cond_init(&render_cond, NULL);
    }

    setFPS(30);

    if (initEgl() < 0)
    {
        COMP_ERROR_MSG("Error getting EGL function addresses");
        goto error;
    }

    x_display = XOpenDisplay(NULL);
    if (NULL == x_display)
    {
        COMP_ERROR_MSG("Error in opening display");
        goto error;
    }

    screen_num = DefaultScreen(x_display);
    if (!width || !height)
    {
        width = DisplayWidth(x_display, screen_num);
        height = DisplayHeight(x_display, screen_num);
        x_offset = 0;
        y_offset = 0;
    }
    COMP_INFO_MSG("Setting Screen width " << width << " height " << height);

    COMP_DEBUG_MSG("Display opened successfully " << (size_t) x_display);

    depth = DefaultDepth(x_display, DefaultScreen(x_display));

    window_attributes.background_pixel =
        BlackPixel(x_display, DefaultScreen(x_display));

    window_attributes.override_redirect = 1;

    x_window = XCreateWindow(x_display,
                             DefaultRootWindow(x_display), x_offset,
                             y_offset, width, height,
                             0,
                             depth, CopyFromParent,
                             CopyFromParent,
                             (CWBackPixel | CWOverrideRedirect),
                             &window_attributes);

    XSelectInput(x_display, (int32_t) x_window, ExposureMask);
    XMapWindow(x_display, (int32_t) x_window);
    gc = XCreateGC(x_display, x_window, 0, NULL);

    XSetForeground(x_display, gc,
                WhitePixel(x_display, DefaultScreen(x_display)) );
    fontinfo = XLoadQueryFont(x_display, "9x15bold");

    if(is_threaded)
    {
		pthread_mutex_lock(&render_lock);
		pthread_create(&render_thread, NULL, renderThread, this);
		pthread_setname_np(render_thread, "NvEglRenderer");
		pthread_cond_wait(&render_cond, &render_lock);
		pthread_mutex_unlock(&render_lock);

		if(isInError())
		{
			pthread_join(render_thread, NULL);
			goto error;
		}
    }

    COMP_DEBUG_MSG("Renderer started successfully")
    return;

error:
    COMP_ERROR_MSG("Got ERROR closing display");
    is_in_error = 1;
}

int
NvEglRenderer::getDisplayResolution(uint32_t &width, uint32_t &height)
{
    int screen_num;
    Display * x_display = XOpenDisplay(NULL);
    if (NULL == x_display)
    {
        return  -1;
    }

    screen_num = DefaultScreen(x_display);
    width = DisplayWidth(x_display, screen_num);
    height = DisplayHeight(x_display, screen_num);

    XCloseDisplay(x_display);
    x_display = NULL;

    return 0;
}


bool
NvEglRenderer::initDirect()
{
	EGLBoolean egl_status;

    static EGLint rgba8888[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE,
    };
    int num_configs = 0;
    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    egl_display = eglGetDisplay(x_display);
    if (EGL_NO_DISPLAY == egl_display)
    {
        COMP_ERROR_MSG("Unable to get egl display");
        goto error;
    }
    COMP_DEBUG_MSG("Egl Got display " << (size_t) egl_display);

    egl_status = eglInitialize(egl_display, 0, 0);
    if (!egl_status)
    {
        COMP_ERROR_MSG("Unable to initialize egl library");
        goto error;
    }

    egl_status =
        eglChooseConfig(egl_display, rgba8888,
                            &egl_config, 1, &num_configs);
    if (!egl_status)
    {
        COMP_ERROR_MSG("Error at eglChooseConfig");
        goto error;
    }
    COMP_DEBUG_MSG("Got numconfigs as " << num_configs);

    egl_context =
        eglCreateContext(egl_display, egl_config,
                            EGL_NO_CONTEXT, context_attribs);
    if (eglGetError() != EGL_SUCCESS)
    {
        COMP_ERROR_MSG("Got Error in eglCreateContext " << eglGetError());
        goto error;
    }
    egl_surface =
        eglCreateWindowSurface(egl_display, egl_config,
                (EGLNativeWindowType) x_window, NULL);
    if (egl_surface == EGL_NO_SURFACE)
    {
        COMP_ERROR_MSG("Error in creating egl surface " << eglGetError());
        goto error;
    }

    eglMakeCurrent(egl_display, egl_surface,
                    egl_surface, egl_context);
    if (eglGetError() != EGL_SUCCESS)
    {
        COMP_ERROR_MSG("Error in eglMakeCurrent " << eglGetError());
        goto error;
    }

    if (InitializeShaders() < 0)
    {
        COMP_ERROR_MSG("Error while initializing shaders");
        goto error;
    }

    create_texture();

    return true;
error:
	is_in_error = 1;
	false;
}


void *
NvEglRenderer::renderThread(void *arg)
{

    NvEglRenderer *renderer = (NvEglRenderer *) arg;
    const char *comp_name = renderer->comp_name;

    assert(renderer->is_threaded);

    bool success = renderer->initDirect();
	if(!success)
		goto error;

    pthread_mutex_lock(&renderer->render_lock);
    pthread_cond_broadcast(&renderer->render_cond);
    COMP_DEBUG_MSG("Starting render thread");

    while (!renderer->isInError() && !renderer->stop_thread)
    {
        pthread_cond_wait(&renderer->render_cond, &renderer->render_lock);
        pthread_mutex_unlock(&renderer->render_lock);

        if(renderer->stop_thread)
        {
            pthread_mutex_lock(&renderer->render_lock);
            break;
        }

        renderer->renderInternal();
        COMP_DEBUG_MSG("Rendered fd=" << renderer->render_fd);

        pthread_mutex_lock(&renderer->render_lock);
        pthread_cond_broadcast(&renderer->render_cond);

    }
    pthread_mutex_unlock(&renderer->render_lock);
    COMP_DEBUG_MSG("Stopped render thread");


finish:

	renderer->finishDirect();

    pthread_mutex_lock(&renderer->render_lock);
    pthread_cond_broadcast(&renderer->render_cond);
    pthread_mutex_unlock(&renderer->render_lock);
    return NULL;

error:
        renderer->is_in_error = 1;
        goto finish;

}

void NvEglRenderer::finishDirect()
{
    EGLBoolean egl_status;

    if (texture_id)
    {
        glDeleteTextures(1, &texture_id);
    }

    if (egl_display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    if (egl_surface != EGL_NO_SURFACE)
    {
        egl_status = eglDestroySurface(egl_display, egl_surface);
        if (egl_status == EGL_FALSE)
        {
            COMP_ERROR_MSG("EGL surface destruction failed");
        }
    }

    if (egl_context != EGL_NO_CONTEXT)
    {
        egl_status = eglDestroyContext(egl_display, egl_context);
        if (egl_status == EGL_FALSE)
        {
            COMP_ERROR_MSG("EGL context destruction failed");
        }
    }

    if (egl_display != EGL_NO_DISPLAY)
    {
        eglReleaseThread();
        eglTerminate(egl_display);
    }
}

NvEglRenderer::~NvEglRenderer()
{
    stop_thread = true;

    if(is_threaded)
    {
		pthread_mutex_lock(&render_lock);
		pthread_cond_broadcast(&render_cond);
		pthread_mutex_unlock(&render_lock);

    	pthread_join(render_thread, NULL);

		pthread_mutex_destroy(&render_lock);
		pthread_cond_destroy(&render_cond);
    }

    if (fontinfo)
    {
        XFreeFont(x_display, fontinfo);
    }

    if (gc)
    {
        XFreeGC(x_display, gc);
    }

    if (x_window)
    {
        XUnmapWindow(x_display, (int32_t) x_window);
        XFlush(x_display);
        XDestroyWindow(x_display, (int32_t) x_window);
    }
    if (x_display)
    {
        XCloseDisplay(x_display);
    }
}

int
NvEglRenderer::render(int fd, uint32_t width, uint32_t height, void* pData)
{
    pRenderData = pData;
    render_fd = fd;
    render_width = width;
    render_height = height;

    if(is_threaded)
    {
		pthread_mutex_lock(&render_lock);
		pthread_cond_broadcast(&render_cond);
		COMP_DEBUG_MSG("Rendering fd=" << fd);
		pthread_cond_wait(&render_cond, &render_lock);
		pthread_mutex_unlock(&render_lock);
		return 0;
    }
    else
    {
    	static bool ranOnce = false;
    	if(!ranOnce)
    	{
    		initDirect();
    		ranOnce = true;
    	}
    	return renderInternal();
    }
}

int
NvEglRenderer::renderInternal()
{
    nvtxRangePushA("Rendering");

    frame_bbox* pBbox = (frame_bbox*) (pRenderData);

    EGLImageKHR hEglImage;
    bool frame_is_late = false;

    EGLSyncKHR egl_sync;
    int iErr;
    hEglImage = NvEGLImageFromFd(egl_display, render_fd);
    if (!hEglImage)
    {
        COMP_ERROR_MSG("Could not get EglImage from fd. Not rendering");
        return -1;
    }

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Frame buffer for GUI
    GLuint texGUI = 0;
    glGenTextures(1, &texGUI);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texGUI);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, render_width, render_height);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texGUI, 0);

    // Clear GUI texture
    glClear(GL_COLOR_BUFFER_BIT);

    if (pBbox && pBbox->g_rect_num > 0)
    {
        for (int i = 0; i < pBbox->g_rect_num; ++i)
        {
            // Blur and blit the image inside bbox to another texture for future processing
            GLuint texBbox = 0;
            glGenTextures(1, &texBbox);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texBbox);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, pBbox->g_rect[i].width, pBbox->g_rect[i].height);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texBbox, 0);

            glViewport(0, 0, pBbox->g_rect[i].width, pBbox->g_rect[i].height);

            glUseProgram(m_programBlur);

            uint32_t pos_location = glGetAttribLocation(m_programBlur,
                                                        "in_pos");
            glEnableVertexAttribArray(pos_location);
            glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0,
                                  kVertices);

            uint32_t tc_location = glGetAttribLocation(m_programBlur, "in_tc");

            // === TGD Exercise ===
            // Very common bug to forget to glEnableVertexAttribArray

            glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0, kTextureCoords);

            glUniform2i(glGetUniformLocation(m_programBlur, "windowSize"), render_width, render_height);

            glUniform4i(glGetUniformLocation(m_programBlur, "bbox"),
                        pBbox->g_rect[i].left, pBbox->g_rect[i].top,
                        pBbox->g_rect[i].width, pBbox->g_rect[i].height);

            glActiveTexture(GL_TEXTURE0);
            glUniform1i(glGetUniformLocation(m_programBlur, "tex"), 0);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
            glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, hEglImage);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glDisableVertexAttribArray(pos_location);
            glDisableVertexAttribArray(tc_location);

            // Use temp render target for next several shaders
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texGUI);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texGUI, 0);
            glViewport(0, 0, render_width, render_height);

            // Draw edge
            glUseProgram(m_programDrawEdge);

            pos_location = glGetAttribLocation(m_programDrawEdge, "in_pos");
            glEnableVertexAttribArray(pos_location);
            glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);

            tc_location = glGetAttribLocation(m_programDrawEdge, "in_tc");

           glEnableVertexAttribArray(tc_location);

            glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0, kTextureCoords);

            glUniform2i(glGetUniformLocation(m_programDrawEdge, "windowSize"), render_width, render_height);

            glUniform4i(glGetUniformLocation(m_programDrawEdge, "bbox"),
                        pBbox->g_rect[i].left, pBbox->g_rect[i].top,
                        pBbox->g_rect[i].width, pBbox->g_rect[i].height);

            glActiveTexture(GL_TEXTURE0);
            glUniform1i(glGetUniformLocation(m_programDrawEdge, "texSampler"), 0);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
            glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, hEglImage);

            glActiveTexture(GL_TEXTURE1);
            glUniform1i(glGetUniformLocation(m_programDrawEdge, "texProcessed"), 1);
            glBindTexture(GL_TEXTURE_2D, texBbox);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glDisableVertexAttribArray(pos_location);
            glDisableVertexAttribArray(tc_location);

            // Draw bbox
            glUseProgram(m_programDrawRect);

            pos_location = glGetAttribLocation(m_programDrawRect, "in_pos");
            glEnableVertexAttribArray(pos_location);
            glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVerticesRect);

            const uint32_t locOffset = glGetUniformLocation(m_programDrawRect, "offset");
            const uint32_t locScale = glGetUniformLocation(m_programDrawRect, "scale");

            float offsetLocalX = float(pBbox->g_rect[i].width) / render_width;
            float offsetLocalY = float(pBbox->g_rect[i].height) / render_height;

            m_offsetLocalX[i] = 0.5 * m_offsetLocalX[i] + 0.5 * offsetLocalX;
            m_offsetLocalY[i] = 0.5 * m_offsetLocalY[i] + 0.5 * offsetLocalY;

            m_offsetWorldX[i] = 2.f * (pBbox->g_rect[i].left + 0.5 * pBbox->g_rect[i].width) / render_width - 1.f;
            m_offsetWorldY[i] = -(2.f * (pBbox->g_rect[i].top + 0.5 * pBbox->g_rect[i].height) / render_height - 1.f);

            const float scale = 0.085;
            const float scaleY = float(render_width) / render_height;

            glUniform2f(locOffset, (-m_offsetLocalX[i] + m_offsetWorldX[i]),
                        (m_offsetLocalY[i] + m_offsetWorldY[i]));
            glUniform2f(locScale, scale, scale * scaleY);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 6);

            glUniform2f(locOffset, (-m_offsetLocalX[i] + m_offsetWorldX[i]),
                        (-m_offsetLocalY[i] + m_offsetWorldY[i]));
            glUniform2f(locScale, scale, -scale * scaleY);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 6);

            glUniform2f(locOffset, (m_offsetLocalX[i] + m_offsetWorldX[i]),
                        (-m_offsetLocalY[i] + m_offsetWorldY[i]));
            glUniform2f(locScale, -scale, -scale * scaleY);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 6);

            glUniform2f(locOffset, (m_offsetLocalX[i] + m_offsetWorldX[i]),
                        (m_offsetLocalY[i] + m_offsetWorldY[i]));
            glUniform2f(locScale, -scale, scale * scaleY);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 6);

            // Clean up
            glDeleteTextures(1, &texBbox);
            glDisableVertexAttribArray(pos_location);
        }
    }

    // Draw the video
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);

    glUseProgram(m_programDrawVideo);

    uint32_t pos_location = glGetAttribLocation(m_programDrawVideo, "in_pos");
    glEnableVertexAttribArray(pos_location);
    glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);

    uint32_t tc_location = glGetAttribLocation(m_programDrawVideo, "in_tc");
    glEnableVertexAttribArray(tc_location);
    glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0, kTextureCoords);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(m_programDrawVideo, "texVideo"), 0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, hEglImage);

    glActiveTexture(GL_TEXTURE1);
    glUniform1i(glGetUniformLocation(m_programDrawVideo, "texGUI"), 1);
    glBindTexture(GL_TEXTURE_2D, texGUI);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Clean up
    glDisableVertexAttribArray(pos_location);
    glDisableVertexAttribArray(tc_location);
    glDeleteTextures(1, &texGUI);
    glDeleteFramebuffers(1, &fbo);

    nvtxRangePop();//Render

    nvtxRangePushA("Frame management");

    // Sync
    egl_sync = eglCreateSyncKHR(egl_display, EGL_SYNC_FENCE_KHR, NULL);
    if (egl_sync == EGL_NO_SYNC_KHR)
    {
    	EGLint eglErr = eglGetError();
        COMP_ERROR_MSG("eglCreateSyncKHR() failed EGL Error:" << eglErr);
        return -1;
    }
    if (last_render_time.tv_sec != 0)
    {
    	if(is_threaded)
    	{
    		pthread_mutex_lock(&render_lock);
    	}
        last_render_time.tv_sec += render_time_sec;
        last_render_time.tv_nsec += render_time_nsec;
        last_render_time.tv_sec += last_render_time.tv_nsec / 1000000000UL;
        last_render_time.tv_nsec %= 1000000000UL;

        if (isProfilingEnabled())
        {
            struct timeval cur_time;
            gettimeofday(&cur_time, NULL);
            if ((cur_time.tv_sec * 1000000.0 + cur_time.tv_usec) >
                    (last_render_time.tv_sec * 1000000.0 +
                     last_render_time.tv_nsec / 1000.0))
            {
                frame_is_late = true;
            }
        }

        if(is_threaded)
        {
			pthread_cond_timedwait(&render_cond, &render_lock, &last_render_time);
	        pthread_mutex_unlock(&render_lock);
        }

    }
    else
    {
        struct timeval now;

        gettimeofday(&now, NULL);
        last_render_time.tv_sec = now.tv_sec;
        last_render_time.tv_nsec = now.tv_usec * 1000L;
    }


    eglSwapBuffers(egl_display, egl_surface);
    if (eglGetError() != EGL_SUCCESS)
    {
        COMP_ERROR_MSG("Got Error in eglSwapBuffers " << eglGetError());
        return -1;
    }

    if (eglClientWaitSyncKHR (egl_display, egl_sync,
                EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR) == EGL_FALSE)
    {
        COMP_ERROR_MSG("eglClientWaitSyncKHR failed!");
    }

    if (eglDestroySyncKHR(egl_display, egl_sync) != EGL_TRUE)
    {
        COMP_ERROR_MSG("eglDestroySyncKHR failed!");
    }
    NvDestroyEGLImage(egl_display, hEglImage);

    if (strlen(overlay_str) != 0)
    {
        XSetForeground(x_display, gc,
                        BlackPixel(x_display, DefaultScreen(x_display)));
        XSetFont(x_display, gc, fontinfo->fid);
        XDrawString(x_display, x_window, gc, overlay_str_x_offset,
                    overlay_str_y_offset, overlay_str, strlen(overlay_str));
    }

    nvtxRangePop();//("Frame management");

    profiler.finishProcessing(0, frame_is_late);


    return 0;
}

int
NvEglRenderer::setOverlayText(char *str, uint32_t x, uint32_t y)
{
    strncpy(overlay_str, str, sizeof(overlay_str));
    overlay_str[sizeof(overlay_str) - 1] = '\0';

    overlay_str_x_offset = x;
    overlay_str_y_offset = y;

    return 0;
}

int
NvEglRenderer::setFPS(float fps)
{
    uint64_t render_time_usec;

    if (fps == 0)
    {
        COMP_WARN_MSG("Fps 0 is not allowed. Not changing fps");
        return -1;
    }

    if(is_threaded)
    {
    	pthread_mutex_lock(&render_lock);
    }

    this->fps = fps;

    render_time_usec = 1000000L / fps;
    render_time_sec = render_time_usec / 1000000;
    render_time_nsec = (render_time_usec % 1000000) * 1000L;

    if(is_threaded)
    {
    	pthread_mutex_unlock(&render_lock);
    }

    return 0;
}

NvEglRenderer *
NvEglRenderer::createEglRenderer(const char *name, uint32_t width,
                               uint32_t height, uint32_t x_offset,
                               uint32_t y_offset, bool threaded)
{
    NvEglRenderer* renderer = new NvEglRenderer(name, width, height,
                                    x_offset, y_offset, threaded);
    if (renderer->isInError())
    {
        delete renderer;
        return NULL;
    }
    return renderer;
}

int
NvEglRenderer::initEgl()
{
    eglCreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    ERROR_GOTO_FAIL(!eglCreateImageKHR,
                    "ERROR getting proc addr of eglCreateImageKHR\n");

    eglDestroyImageKHR =
        (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
    ERROR_GOTO_FAIL(!eglDestroyImageKHR,
                    "ERROR getting proc addr of eglDestroyImageKHR\n");

    eglCreateSyncKHR =
        (PFNEGLCREATESYNCKHRPROC) eglGetProcAddress("eglCreateSyncKHR");
    ERROR_GOTO_FAIL(!eglCreateSyncKHR,
                    "ERROR getting proc addr of eglCreateSyncKHR\n");

    eglDestroySyncKHR =
        (PFNEGLDESTROYSYNCKHRPROC) eglGetProcAddress("eglDestroySyncKHR");
    ERROR_GOTO_FAIL(!eglDestroySyncKHR,
                    "ERROR getting proc addr of eglDestroySyncKHR\n");

    eglClientWaitSyncKHR =
        (PFNEGLCLIENTWAITSYNCKHRPROC) eglGetProcAddress("eglClientWaitSyncKHR");
    ERROR_GOTO_FAIL(!eglClientWaitSyncKHR,
                    "ERROR getting proc addr of eglClientWaitSyncKHR\n");

    eglGetSyncAttribKHR =
        (PFNEGLGETSYNCATTRIBKHRPROC) eglGetProcAddress("eglGetSyncAttribKHR");
    ERROR_GOTO_FAIL(!eglGetSyncAttribKHR,
                    "ERROR getting proc addr of eglGetSyncAttribKHR\n");

    glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
        eglGetProcAddress("glEGLImageTargetTexture2DOES");
    ERROR_GOTO_FAIL(!glEGLImageTargetTexture2DOES,
                    "ERROR getting proc addr of glEGLImageTargetTexture2DOES\n");

    return 0;

fail:
    return -1;
}

void
NvEglRenderer::CreateShader(GLuint program, GLenum type, const char *source,
        int size)
{

    char log[4096];
    int result = GL_FALSE;

    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, &size);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        COMP_DEBUG_MSG("Got Fatal Log as " << log);
    }
    glAttachShader(program, shader);

    if (glGetError() != GL_NO_ERROR)
    {
        COMP_ERROR_MSG("Got gl error as " << glGetError());
    }
}

int
NvEglRenderer::InitializeShaders(void)
{
    int result = GL_FALSE;

    auto CreateProgram =
            [this](const std::string& vsSource, const std::string& fsSource) -> GLuint
            {
                GLuint program = glCreateProgram();

                CreateShader(program, GL_VERTEX_SHADER, vsSource.c_str(), vsSource.size());

                CreateShader(program, GL_FRAGMENT_SHADER, fsSource.c_str(), fsSource.size());

                glLinkProgram(program);

                int result = GL_FALSE;
                glGetProgramiv(program, GL_LINK_STATUS, &result);

                if (!result)
                {
                    char log[4096];
                    glGetShaderInfoLog(program, sizeof(log), NULL, log);
                    COMP_ERROR_MSG("Error while Linking " << log);
                    std::cout << log << std::endl;
                    return 0;
                }

                return program;
            };

    static const std::string kVertexShader =
            R"(
#version 310
in vec4 in_pos;
in vec2 in_tc;
out vec2 interp_tc;

void main() 
{
    interp_tc = in_tc;
    gl_Position = in_pos;
}
    )";

    //------------------------------------------
    // Draw Video
    //------------------------------------------
    static const std::string kFragmentShaderVideo =
            R"(
#version 310
#extension GL_OES_EGL_image_external : require
precision mediump float;

in vec2 interp_tc;
out vec4 out_color;
uniform samplerExternalOES texVideo;
uniform sampler2D texGUI;

void main()
{
    vec3 colorVideo = texture2D(texVideo, interp_tc).rgb;
    vec4 colorGUI = texture(texGUI, vec2(0, 1) + vec2(1, -1) * interp_tc);

    out_color = vec4(colorVideo * vec3(1.f - colorGUI.a, 1.f, 1.f) + colorGUI .rgb * vec3(colorGUI.a), 1.f);
}
    )";

    m_programDrawVideo = CreateProgram(kVertexShader, kFragmentShaderVideo);

    if (!m_programDrawVideo)
    {
        return -1;
    }

    //------------------------------------------
    // Edge Detection
    //------------------------------------------
    static const std::string kFragmentShaderEdge =
            R"(
#version 310
precision mediump float;

in vec2 interp_tc;
out vec4 out_color;
uniform sampler2D texProcessed;
uniform ivec2 windowSize;
uniform ivec4 bbox;
mat3 sx = mat3(1.0, 2.0, 1.0, 0.0, 0.0, 0.0, -1.0, -2.0, -1.0);
mat3 sy = mat3(1.0, 0.0, -1.0, 2.0, 0.0, -2.0, 1.0, 0.0, -1.0);

void main()
{
    ivec2 samplePos = ivec2(interp_tc * vec2(windowSize));

    if (samplePos.x > bbox.x &&
        samplePos.x < bbox.x + bbox.z &&
        samplePos.y > bbox.y &&
        samplePos.y < bbox.y + bbox.w)
    {
        mat3 I;

        ivec2 fetchTexCoord = ivec2(0, bbox.w) + ivec2(1, -1) * (samplePos - bbox.xy);
        vec2 texCoord = vec2(fetchTexCoord) / vec2(bbox.z, bbox.w);

        // Canny edge detecting
        vec3 diffuse = texelFetch(texProcessed, fetchTexCoord, 0).rgb;

        for(int i = 0; i < 3;i++)
        {
            for(int j = 0; j < 3; j++)
            {
                ivec2 deltCoord = ivec2(i - 1, j - 1);
                ivec2 fetchTexCoordClamped = clamp(fetchTexCoord + deltCoord, ivec2(0, 0), ivec2(bbox.z - 1, bbox.w - 1));
                vec3 val = texelFetch(texProcessed, fetchTexCoordClamped, 0).rgb;
                I[i][j] = length(val);
            }
        }

        float gx = dot(sx[0], I[0])+ dot(sx[1], I[1]) + dot(sx[2], I[2]);
        float gy = dot(sy[0], I[0])+ dot(sy[1], I[1]) + dot(sy[2], I[2]);

        float g = sqrt(pow(gx, 2.0) + pow(gy, 2.0));

        // === TGD Exercise ===
        // Fine tune the threshold)"
            R"(
        if (g > 0.25))"
            R"(
        {
            out_color = vec4(1.f, 0.f, 0.f, 1.f);
        }
        else
        {
            discard;
        }

        //out_color = vec4(diffuse, 1);
    }
    else
    {
        discard;
    }
}
    )";

    m_programDrawEdge = CreateProgram(kVertexShader, kFragmentShaderEdge);

    if (!m_programDrawEdge)
    {
        return -1;
    }

    //--------------------------------------
    // Draw rect
    //--------------------------------------
    static const std::string kVertexShaderDrawRect =
            R"(
attribute vec4 in_pos;
uniform vec2 offset;
uniform vec2 scale;

void main() 
{
    gl_Position = in_pos * vec4(scale, 1, 1) + vec4(offset, 0, 0);
}
    )";

    static const std::string kFragmentShaderDrawRect =
            R"(
precision mediump float;

void main()
{
    gl_FragColor = vec4(116.0 / 255.0, 183.0 / 255.0, 27.0 / 255.0, 1.0);
}
    )";

    m_programDrawRect = CreateProgram(kVertexShaderDrawRect,
                                      kFragmentShaderDrawRect);

    if (!m_programDrawRect)
    {
        return -1;
    }

    //------------------------------------------
    // Blur
    //------------------------------------------
    static const std::string kFragmentShaderBlur =
            R"(
#version 310
#extension GL_OES_EGL_image_external : require
precision highp float;

in vec2 interp_tc;
out vec4 out_color;
uniform samplerExternalOES tex;
uniform ivec2 windowSize;
uniform ivec4 bbox;

vec2 offsets[25] = vec2[](
    vec2(-2, -2), vec2(-1, -2), vec2(0, -2), vec2(1, -2), vec2(2, -2),
    vec2(-2, -1), vec2(-1, -1), vec2(0, -1), vec2(1, -1), vec2(2, -1),
    vec2(-2,  0), vec2(-1,  0), vec2(0,  0), vec2(1,  0), vec2(2,  0),
    vec2(-2,  1), vec2(-1,  1), vec2(0,  1), vec2(1,  1), vec2(2,  1),
    vec2(-2,  2), vec2(-1,  2), vec2(0,  2), vec2(1,  2), vec2(2,  2)
);

float gaussianWeight[25] = float[25](
    0.003765, 0.015019, 0.023792, 0.015019, 0.003765,
    0.015019, 0.059912, 0.094907, 0.059912, 0.015019,
    0.023792, 0.094907, 0.150342, 0.094907, 0.023792,
    0.015019, 0.059912, 0.094907, 0.059912, 0.015019,
    0.003765, 0.015019, 0.023792, 0.015019, 0.003765
);

float normpdf(float x, float sigma)
{
    return 0.39894 * exp(-0.5 * x * x / (sigma * sigma)) / sigma;
}

float normpdf3(vec3 v, float sigma)
{
    return 0.39894 * exp(-0.5 * dot(v, v) / (sigma * sigma)) / sigma;
}

vec4 blur_gaussian(samplerExternalOES image, vec2 uv) {
    vec4 finalColor = vec4(0.f);
    float normalization = 0.f;

    for (int i = 0; i < 25; ++i) {
        vec4 color = texture2D(image, uv + offsets[i] / vec2(windowSize));
        float weight = gaussianWeight[i];
        finalColor += weight * color;
        normalization += weight;
    }

    return vec4((finalColor / normalization).rgb, 1.f);
}

vec4 blur_bilateral(samplerExternalOES image, vec2 uv) {
    
    vec3 centreColour = texture2D(image, uv).rgb;

    vec3 finalColor = vec3(0.f);
    float normalization = 0.f;

    // === TGD Exercise ===
    // Optimize the shader by setting this smaller)"
            R"(
    const int KSize = 20;)"
            R"(

    const int KLength = (KSize - 1) / 2;

    float kernel[KSize];

    float bZ = 1.f / normpdf(0.0, 0.1);

    for (int i = 0; i <= KLength; ++i) {
        kernel[KLength + i] = kernel[KLength - i] = normpdf(float(i), 9.f);
    }

    for (int i = -KLength; i <= KLength; ++i) {
        for (int j = -KLength; j <= KLength; ++j) {
            vec2 tt = uv + vec2(i, j) / vec2(windowSize);
            vec3 cc = texture2D(image, tt).rgb;
            float factor = normpdf3(cc - centreColour, 0.1) * bZ * kernel[KLength + j] * kernel[KLength + i];
            normalization += factor;
            finalColor += factor * cc;
        }
    }

    return vec4((finalColor / normalization).rgb, 1.f);
}

void main()
{
    vec2 texCoord = (interp_tc * vec2(bbox.z, bbox.w) + vec2(bbox.x, bbox.y)) / vec2(windowSize);
    // === TGD Exercise ===
    // Try other algorithm (blur_bilateral))"
        R"(
    out_color = blur_gaussian(tex, texCoord);)"
        R"(
}
    )";

    m_programBlur = CreateProgram(kVertexShader, kFragmentShaderBlur);

    if (!m_programBlur)
    {
        return -1;
    }

    COMP_DEBUG_MSG("Shaders intialized");

    return 0;
}

int
NvEglRenderer::create_texture()
{
    glGetIntegerv(GL_VIEWPORT, m_viewport);
    glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
    glScissor(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);

    glGenTextures(1, &texture_id);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
    return 0;
}
