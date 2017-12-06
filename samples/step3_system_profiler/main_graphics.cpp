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

#include <errno.h>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <linux/videodev2.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "NvUtils.h"
#include "nvbuf_utils.h"
#include "v4l2_nv_extensions.h"
#include "v4l2_backend_test.h"

using namespace std;

#ifdef ENABLE_TENSORRT_THREAD
#define OSD_BUF_NUM 100
#endif

void *render_thread(void* arg)
{
    context_t *ctx = (context_t *) arg;
    Shared_Buffer render_buffer;
#ifdef ENABLE_TENSORRT_THREAD
    frame_bbox *bbox = NULL;
    frame_bbox temp_bbox;
    temp_bbox.g_rect_num = 0;
    temp_bbox.g_rect = new NvOSD_RectParams[OSD_BUF_NUM];
#endif

    while (1)
    {
        nvtxRangePushA("Wait for image");

        // waiting for buffer to come
        pthread_mutex_lock(&ctx->render_lock);
        if (ctx->render_buf_queue->empty())
        {
            if (ctx->stop_render)
            {
                pthread_mutex_unlock(&ctx->render_lock);
                break;
            }
            else
            {
                // wait 3 s for frame to come
                struct timespec timeout;
                timeout.tv_sec = time(0) + 3;
                timeout.tv_nsec = 0;

#ifdef RENDERER_INF_WAIT
                int ret = pthread_cond_wait(&ctx->render_cond, &ctx->render_lock);
#else
                int ret = pthread_cond_timedwait(&ctx->render_cond, &ctx->render_lock, &timeout);
#endif

                // if no signal come in 3s, exit.
                if (ret != 0)
                {
                    pthread_mutex_unlock(&ctx->render_lock);
                    break;
                }
                // if no buffer come, we can exit now.
                if (ctx->render_buf_queue->empty())
                {
                    pthread_mutex_unlock(&ctx->render_lock);
                    break;
                }
            }
        }
        //pop up buffer from queue to process
        render_buffer = ctx->render_buf_queue->front();
        ctx->render_buf_queue->pop();
        pthread_mutex_unlock(&ctx->render_lock);

        nvtxRangePop(); //("Wait for image");


        struct v4l2_buffer *v4l2_buf = &render_buffer.v4l2_buf;
        NvBuffer *buffer             = render_buffer.buffer;

        if (ctx->cpu_occupation_option != PARSER_DECODER_VIC)
        {
            // Render thread is waiting for result, wait here
            if (render_buffer.bProcess  == 1)
            {
                nvtxRangePush("Wait for recognition rects");

                sem_wait(&ctx->result_ready_sem);
                pthread_mutex_lock(&ctx->osd_lock);
                if (ctx->osd_queue->size() != 0)
                {
                    bbox = ctx->osd_queue->front();
                    if (bbox != NULL)
                    {
                        temp_bbox.g_rect_num = bbox->g_rect_num;
                        memcpy(temp_bbox.g_rect, bbox->g_rect,
                            OSD_BUF_NUM * sizeof(NvOSD_RectParams));
                        delete []bbox->g_rect;
                        delete bbox;
                        bbox = NULL;
                    }
                    ctx->osd_queue->pop();
                }
                pthread_mutex_unlock(&ctx->osd_lock);

                nvtxRangePop();
            }

#ifdef ENABLE_NVOSD
            //We do not need to use NVOSD except for debugging, NvEglRenderer now covers this
            if (temp_bbox.g_rect_num != 0)
            {
                nvtxRangePushA("Rendering labels (NVOSD)");
                nvosd_draw_rectangles(ctx->nvosd_context, MODE_HW,
                    buffer->planes[0].fd, temp_bbox.g_rect_num, temp_bbox.g_rect);
                nvtxRangePop();
            }
#endif
            nvtxRangePushA("Rendering dataset");

            // EglRenderer requires the fd of the 0th plane to render
            ctx->renderer->render(buffer->planes[0].fd, buffer->planes[0].fmt.width, buffer->planes[0].fmt.height, &temp_bbox);

            nvtxRangePop();

            // Write raw video frame to file and return the buffer to converter
            // capture plane
            if (ctx->out_file)
                write_video_frame(ctx->out_file, *buffer);
        }

        if (ctx->conv->capture_plane.qBuffer(*v4l2_buf, NULL) < 0)
        {
            return NULL;
        }
    }

#ifdef RENDER_DIRECT
    ctx->renderer->finishDirect();
#endif

#ifdef ENABLE_TENSORRT_THREAD
    delete []temp_bbox.g_rect;
#endif
    return NULL;
}


