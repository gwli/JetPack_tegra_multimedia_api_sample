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
#include <cassert>

#include "NvUtils.h"
#include "NvAnalysis.h"
#include "nvbuf_utils.h"
#include "v4l2_nv_extensions.h"
#include "v4l2_backend_test.h"

using namespace std;

#ifdef ENABLE_TENSORRT_THREAD
TENSORRT_Context g_tensorrt_context;

static void wait_for_input(Shared_Buffer& tensorrt_buffer, int& process_last_batch);
static bool prepare_input(int& buf_num, Shared_Buffer& tensorrt_buffer);
static void recognize_objects(Shared_Buffer& tensorrt_buffer);

void *tensorrt_thread(void *data)
{
    int buf_num = 0;
    int process_last_batch = 0;
    Shared_Buffer tensorrt_buffer;

    while (process_last_batch == 0)
    {
    	//NVTX EXAMPLE
    	//nvtxRangePush("message");
    	//nvtxRangePop();

    	nvtxRangePush("wait_for_input");
    	wait_for_input(tensorrt_buffer, process_last_batch);
    	nvtxRangePop();

    	if (process_last_batch != 0 && buf_num == 0)
            break;


    	nvtxRangePush("prepare_input");
		prepare_input(buf_num, tensorrt_buffer);
		nvtxRangePop();


    	// now we push it to capture plane to let v4l2 go on
		// buffer is not enough for a batch, continue to wait for buffer
		if (buf_num < g_tensorrt_context.getBatchSize())
		{
			continue;
		}


		nvtxRangePush("recognize_objects");
        recognize_objects(tensorrt_buffer);
    	nvtxRangePop();


        buf_num = 0;
   } //END OF THREAD LOOP

    return NULL;
}


#ifdef ENABLE_TENSORRT
using namespace nvinfer1;
using namespace nvcaffeparser1;
#endif

static const int IMAGE_WIDTH = 1920;
static const int IMAGE_HEIGHT = 1080;

#ifdef ENABLE_TENSORRT_THREAD
#define OSD_BUF_NUM 100
int frame_num = 1;  //this is used to filter image feeding to TENSORRT

//following structure is used to communicate between TENSORRT thread and
//V4l2 capture thread
queue<Shared_Buffer> TENSORRT_Buffer_Queue;
pthread_mutex_t      TENSORRT_lock; // for dec and conv
pthread_cond_t       TENSORRT_cond;
int                  TENSORRT_Stop = 0;
pthread_t            TENSORRT_Thread_handle;

#endif

EGLDisplay egl_display;


static void wait_for_input(Shared_Buffer& tensorrt_buffer, int& process_last_batch)
{
	// wait for buffer for process to come
	pthread_mutex_lock(&TENSORRT_lock);
	if(TENSORRT_Buffer_Queue.empty())
	{
		if (TENSORRT_Stop)
		{
			pthread_mutex_unlock(&TENSORRT_lock);
			return;
		}
		else
		{
			// wait 3 s for frame to come
			struct timespec timeout;
			timeout.tv_sec = time(0) + 3;
			timeout.tv_nsec = 0;
#ifdef TENSORRT_INF_WAIT
			int ret = pthread_cond_wait(&TENSORRT_cond, &TENSORRT_lock);
#else
			int ret = pthread_cond_timedwait(&TENSORRT_cond, &TENSORRT_lock, &timeout);
#endif
			if (ret != 0)
			{
				cout << "Timedout waiting for input data"<<endl;
			}
		}
	}
	if (TENSORRT_Buffer_Queue.size() != 0)
	{
		tensorrt_buffer = TENSORRT_Buffer_Queue.front();
		TENSORRT_Buffer_Queue.pop();
	}
	else
	{
		process_last_batch = 1;
	}
	pthread_mutex_unlock(&TENSORRT_lock);
}

//return success
static bool prepare_input(int& buf_num, Shared_Buffer& tensorrt_buffer)
{
	EGLImageKHR egl_image = NULL;
	NvBuffer * buffer = tensorrt_buffer.buffer;

	int batch_offset = buf_num  * g_tensorrt_context.getNetWidth() *
        g_tensorrt_context.getNetHeight() * g_tensorrt_context.getChannel();

#if LAB_INSTRUCTOR_FORCE_CPU_TENSORRT_PREP

	// copy with CPU is much slower than GPU
	// but still keep it just in case customer want to save GPU
	//generate input buffer for first time
	unsigned char *data = buffer->planes[0].data;
	int channel_offset = g_tensorrt_context.getNetWidth() * g_tensorrt_context.getNetHeight();

	// copy buffer into input_buf
	for (int i = 0; i < g_tensorrt_context.getChannel(); i++)
	{
		for (int j = 0; j < g_tensorrt_context.getNetHeight(); j++)
		{
			for (int k = 0; k < g_tensorrt_context.getNetWidth(); k++)
			{
				int dst_offset = batch_offset + channel_offset * i +
					j * g_tensorrt_context.getNetWidth() + k;

				int src0 = (int)*(data + ((2*j+0) * buffer->planes[0].fmt.stride + (2*k+0) * 4 + 3 - i - 1));
				int src1 = (int)*(data + ((2*j+1) * buffer->planes[0].fmt.stride + (2*k+0) * 4 + 3 - i - 1));
				int src2 = (int)*(data + ((2*j+0) * buffer->planes[0].fmt.stride + (2*k+1) * 4 + 3 - i - 1));
				int src3 = (int)*(data + ((2*j+1) * buffer->planes[0].fmt.stride + (2*k+1) * 4 + 3 - i - 1));

				float blend = 0.25f * (float)(src0+src1+src2+src3);

				tensorrt_inputbuf[dst_offset] = blend;
			}
		}
	}
#else
	// map fd into EGLImage, then copy it with GPU in parallel
	// Create EGLImage from dmabuf fd
	egl_image = NvEGLImageFromFd(egl_display,
									buffer->planes[0].fd);
	if (egl_image == NULL)
	{
		cerr << "Error while mapping dmabuf fd (" <<
			buffer->planes[0].fd << ") to EGLImage" << endl;
		return false;
	}

	void *cuda_buf = g_tensorrt_context.getBuffer(0);
	// map eglimage into GPU address
	bool status = prepareEGLImage2FloatForTensorRt(
		&egl_image,
		g_tensorrt_context.getNetWidth(),
		g_tensorrt_context.getNetHeight(),
		(TENSORRT_MODEL == GOOGLENET_THREE_CLASS) ? COLOR_FORMAT_BGR : COLOR_FORMAT_RGB,
		(char *)cuda_buf + batch_offset * sizeof(float));

	if (!status)
		exit(1);

	// Destroy EGLImage
	NvDestroyEGLImage(egl_display, egl_image);
	egl_image = NULL;
#endif

    buf_num++;

	return true;
}

static void recognize_objects(Shared_Buffer& tensorrt_buffer)
{
	context_t* ctx = (context_t *)tensorrt_buffer.arg;
    float *tensorrt_inputbuf = g_tensorrt_context.getInputBuf();

    int classCnt = g_tensorrt_context.getModelClassCnt();
    queue<vector<cv::Rect>> rectList_queue[classCnt];

#if ENABLE_TENSORRT
#if LAB_INSTRUCTOR_FORCE_CPU_TENSORRT_PREP
     g_tensorrt_context.doInference(rectList_queue, tensorrt_inputbuf);
#else
     g_tensorrt_context.doInference(rectList_queue);
#endif
#else
     //We will use TensorRT here and then consume it's output.
     //Let's make a temporary rectangle to render until TensorRT is plugged in.
     //BEGIN TEMP CODE
     std::vector<cv::Rect> rects;
     cv::Rect r(100,100,100,100);
     rects.push_back(r);
     for (int class_num = 0; class_num < classCnt; class_num++)
     {
         rectList_queue[class_num].push(rects);
     }
     //END TEMP CODE
#endif

     for (int i = 0; i < classCnt; i++)
     {
         assert(rectList_queue[i].size() == g_tensorrt_context.getBatchSize());
     }

     while (!rectList_queue[0].empty())
     {
         int rectNum = 0;
         frame_bbox *bbox = new frame_bbox;
         bbox->g_rect_num = 0;
         bbox->g_rect = new NvOSD_RectParams[OSD_BUF_NUM];

         for (int class_num = 0; class_num < classCnt; class_num++)
         {
             vector<cv::Rect> rectList = rectList_queue[class_num].front();
             rectList_queue[class_num].pop();
             for (int i = 0; i < rectList.size(); i++)
             {
                 cv::Rect &r = rectList[i];
                 if ((r.width * IMAGE_WIDTH / g_tensorrt_context.getNetWidth() < 10) ||
                     (r.height * IMAGE_HEIGHT / g_tensorrt_context.getNetHeight() < 10))
                     continue;
                 bbox->g_rect[rectNum].left =
                     (unsigned int) (r.x * IMAGE_WIDTH / g_tensorrt_context.getNetWidth());
                 bbox->g_rect[rectNum].top =
                     (unsigned int) (r.y * IMAGE_HEIGHT / g_tensorrt_context.getNetHeight());
                 bbox->g_rect[rectNum].width =
                     (unsigned int) (r.width * IMAGE_WIDTH / g_tensorrt_context.getNetWidth());
                 bbox->g_rect[rectNum].height =
                     (unsigned int) (r.height * IMAGE_HEIGHT / g_tensorrt_context.getNetHeight());
                 bbox->g_rect[rectNum].border_width = 5;
                 bbox->g_rect[rectNum].has_bg_color = 0;
                 bbox->g_rect[rectNum].border_color.red = ((class_num == 0) ? 1.0f : 0.0);
                 bbox->g_rect[rectNum].border_color.green = ((class_num == 1) ? 1.0f : 0.0);
                 bbox->g_rect[rectNum].border_color.blue = ((class_num == 2) ? 1.0f : 0.0);
                 rectNum++;
             }
         }
         bbox->g_rect_num = rectNum;
         pthread_mutex_lock(&ctx->osd_lock);
         ctx->osd_queue->push(bbox);
         pthread_mutex_unlock(&ctx->osd_lock);
         //TENSORRT has prepared result, notify here
         sem_post(&ctx->result_ready_sem);
     }
}

#endif

