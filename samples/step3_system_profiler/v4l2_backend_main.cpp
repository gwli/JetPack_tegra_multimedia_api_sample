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

const char *GOOGLE_NET_DEPLOY_NAME =
        "../../data/Model/GoogleNet_one_class/GoogleNet_modified_oneClass_halfHD.prototxt";
const char *GOOGLE_NET_MODEL_NAME =
        "../../data/Model/GoogleNet_one_class/GoogleNet_modified_oneClass_halfHD.caffemodel";

using namespace std;

#ifdef ENABLE_TENSORRT_THREAD
#define OSD_BUF_NUM 100
extern int frame_num;  //this is used to filter image feeding to TENSORRT

//following structure is used to communicate between TENSORRT thread and
//V4l2 capture thread
extern queue<Shared_Buffer> TENSORRT_Buffer_Queue;
extern pthread_mutex_t      TENSORRT_lock; // for dec and conv
extern pthread_cond_t       TENSORRT_cond;
extern int                  TENSORRT_Stop;
extern pthread_t            TENSORRT_Thread_handle;

#ifdef ENABLE_TENSORRT
using namespace nvinfer1;
using namespace nvcaffeparser1;
#endif

extern TENSORRT_Context g_tensorrt_context;

#endif

extern EGLDisplay egl_display;

static bool
conv_output_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                   NvBuffer * buffer, NvBuffer * shared_buffer,
                                   void *arg)
{
    context_t *ctx = (context_t *) arg;
    struct v4l2_buffer dec_capture_ret_buffer;
    struct v4l2_plane planes[MAX_PLANES];

    memset(&dec_capture_ret_buffer, 0, sizeof(dec_capture_ret_buffer));
    memset(planes, 0, sizeof(planes));

    dec_capture_ret_buffer.index = shared_buffer->index;
    dec_capture_ret_buffer.m.planes = planes;

    pthread_mutex_lock(&ctx->queue_lock);
    ctx->conv_output_plane_buf_queue->push(buffer);

    // Return the buffer dequeued from converter output plane
    // back to decoder capture plane
    if (ctx->dec->capture_plane.qBuffer(dec_capture_ret_buffer, NULL) < 0)
    {
        pthread_cond_broadcast(&ctx->queue_cond);
        pthread_mutex_unlock(&ctx->queue_lock);
        return false;
    }

    pthread_cond_broadcast(&ctx->queue_cond);
    pthread_mutex_unlock(&ctx->queue_lock);

    return true;
}

static bool
conv_capture_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                    NvBuffer * buffer,
                                    NvBuffer * shared_buffer,
                                    void *arg)
{
    static bool ranOnce = false;
    if(ranOnce == true)
    {
        nvtxRangePop();
    }
    else
    {
        ranOnce = true;
    }
    nvtxRangePushA("V4L2 VIC Block to Pitch");


    context_t *ctx = (context_t *) arg;
    Shared_Buffer batch_buffer;
    batch_buffer.bProcess = 0;
#ifdef ENABLE_TENSORRT_THREAD
    frame_num++;
    //push buffer to process queue
    Shared_Buffer tensorrt_buffer;

    // v4l2_buf is local in the DQthread and exists in the scope of the callback
    // function only and not in the entire application. The application has to
    // copy this for using at out of the callback.
    memcpy(&tensorrt_buffer.v4l2_buf, v4l2_buf, sizeof(v4l2_buffer));

    tensorrt_buffer.buffer = buffer;
    tensorrt_buffer.shared_buffer = shared_buffer;
    tensorrt_buffer.arg = arg;
    pthread_mutex_lock(&TENSORRT_lock);
    TENSORRT_Buffer_Queue.push(tensorrt_buffer);
    pthread_cond_broadcast(&TENSORRT_cond);
    pthread_mutex_unlock(&TENSORRT_lock);
    batch_buffer.bProcess = 1;
#endif //ENABLE_TENSORRT_THREAD

    // v4l2_buf is local in the DQthread and exists in the scope of the callback
    // function only and not in the entire application. The application has to
    // copy this for using at out of the callback.
    memcpy(&batch_buffer.v4l2_buf, v4l2_buf, sizeof(v4l2_buffer));

    batch_buffer.buffer = buffer;
    batch_buffer.shared_buffer = shared_buffer;
    batch_buffer.arg = arg;
    pthread_mutex_lock(&ctx->render_lock);
    ctx->render_buf_queue->push(batch_buffer);
    pthread_cond_broadcast(&ctx->render_cond);
    pthread_mutex_unlock(&ctx->render_lock);

    return true;
}

static void
set_defaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));
    ctx->fullscreen = false;
    ctx->window_height = 0;
    ctx->window_width = 0;
    ctx->window_x = 0;
    ctx->window_y = 0;
    ctx->input_nalu = 1;
    ctx->fps = 30;
    ctx->do_stat = 0;
    ctx->cpu_occupation_option = 0;
    ctx->dec_status = 0;
    ctx->conv_output_plane_buf_queue = new queue < NvBuffer * >;
#ifdef ENABLE_TENSORRT_THREAD
    pthread_mutex_init(&ctx->osd_lock, NULL);
    ctx->osd_queue = new queue <frame_bbox*>;
#endif
    ctx->render_buf_queue = new queue <Shared_Buffer>;
    ctx->stop_render = 0;
    ctx->frame_info_map = new map< uint64_t, frame_info_t* >;
    ctx->nvosd_context = NULL;
}

#ifdef ENABLE_TENSORRT_THREAD
static void
set_globalcfg_default(global_cfg *cfg)
{
    cfg->deployfile = GOOGLE_NET_DEPLOY_NAME;
    cfg->modelfile = GOOGLE_NET_MODEL_NAME;
}
#endif

static void
get_disp_resolution(display_resolution_t *res)
{
    NVTX_RANGE_PUSH("Get display resolution",1);
    if (NvEglRenderer::getDisplayResolution(
            res->window_width, res->window_height) < 0)
    {
        cerr << "get resolution failed, program will exit" << endl;
        exit(0);
    }
    NVTX_RANGE_POP; // End get display resolution

    return;
}

static void
do_statistic(context_t * ctx)
{
    uint64_t accu_latency = 0;
    uint64_t accu_frames = 0;
    struct timeval start_time;
    struct timeval end_time;
    map<uint64_t, frame_info_t*>::iterator  iter;

    memset(&start_time, 0, sizeof(start_time));
    memset(&end_time, 0, sizeof(end_time));
    for( iter = ctx->frame_info_map->begin();
            iter != ctx->frame_info_map->end(); iter++)
    {
        if (iter->second->output_time.tv_sec != 0 &&
                iter->second->input_time.tv_sec != 0)
        {
            accu_latency += (iter->second->output_time.tv_sec -
                                iter->second->input_time.tv_sec) * 1000 +
                               ( iter->second->output_time.tv_usec -
                                iter->second->input_time.tv_usec ) / 1000;
            accu_frames++;
            end_time = iter->second->output_time;
        }

        if (iter == ctx->frame_info_map->begin())
        {
            start_time = iter->second->input_time;
        }
    }

    cout << "total frames:" << accu_frames<<endl;
    cout <<"pipeline:" << ctx->channel << " avg_latency:(ms)" <<
                accu_latency / accu_frames << " fps:" <<
                accu_frames * 1000000 / (end_time.tv_sec * 1000000 +
                end_time.tv_usec - start_time.tv_sec * 1000000 -
                start_time.tv_usec ) << endl;
}

int
main(int argc, char *argv[])
{
    context_t ctx[CHANNEL_NUM];
    global_cfg cfg;
    int error = 0;
    int iterator;
    map<uint64_t, frame_info_t*>::iterator  iter;
    display_resolution_t disp_info;
    char **argp;
    memset(&cfg, 0, sizeof(global_cfg));
#ifdef ENABLE_TENSORRT_THREAD
    set_globalcfg_default(&cfg);
#endif

    argp = argv;
    parse_global(&cfg, argc, &argp);

    if (parse_csv_args(&ctx[0],
#ifdef ENABLE_TENSORRT_THREAD
        &g_tensorrt_context,
#endif
        argc - cfg.channel_num - 1, argp))
    {
        fprintf(stderr, "Error parsing commandline arguments\n");
        return -1;
    }

NVTX_RANGE_PUSH("Build Tensor RT context",1);
#ifdef ENABLE_TENSORRT_THREAD
    g_tensorrt_context.setModelIndex(TENSORRT_MODEL);
#if LAB_INSTRUCTOR_FORCE_CPU_TENSORRT_PREP
    g_tensorrt_context.buildTensorRtContext(cfg.deployfile, cfg.modelfile, true);
#else
    g_tensorrt_context.buildTensorRtContext(cfg.deployfile, cfg.modelfile);
#endif
    NVTX_RANGE_POP; // End build context
    //Batchsize * FilterNum should be not bigger than buffers allocated by VIC
    if (g_tensorrt_context.getBatchSize() * g_tensorrt_context.getFilterNum() > 10)
    {
        fprintf(stderr,
            "Not enough buffers. Decrease tensorrt-proc-interval and run again. Exiting\n");

        NVTX_RANGE_PUSH("Destroy Tensor RT context",1);
#if LAB_INSTRUCTOR_FORCE_CPU_TENSORRT_PREP
        g_tensorrt_context.destroyTensorRtContext(true);
#else
        g_tensorrt_context.destroyTensorRtContext();
#endif
        NVTX_RANGE_POP; // End destroy context
        return 0;
    }
#endif

#ifdef ENABLE_TENSORRT_THREAD
    pthread_create(&TENSORRT_Thread_handle, NULL, tensorrt_thread, NULL);
    pthread_setname_np(TENSORRT_Thread_handle, "tensorrt_thread");
#endif

    get_disp_resolution(&disp_info);
    init_decode_ts();

    NVTX_RANGE_PUSH("Init EGL",1);
    // Get defalut EGL display
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY)
    {
        cout<<"Error while get EGL display connection"<<endl;
        return -1;
    }

    // Init EGL display connection
    if (!eglInitialize(egl_display, NULL, NULL))
    {
        cout<<"Error while initialize EGL display connection"<<endl;
        return -1;
    }
    NVTX_RANGE_POP;// End init EGL

    for (iterator = 0; iterator < cfg.channel_num; iterator++)
    {
        int ret = 0;
        sem_init(&(ctx[iterator].dec_run_sem), 0, 0);
#ifdef ENABLE_TENSORRT_THREAD
        sem_init(&(ctx[iterator].result_ready_sem), 0, 0);
#endif
        set_defaults(&ctx[iterator]);

        char decname[512];
        sprintf(decname, "dec%d", iterator);
        ctx[iterator].channel = iterator;

        NVTX_RANGE_PUSH("Create video decoder",2);
        if (parse_csv_args(&ctx[iterator],
#ifdef ENABLE_TENSORRT_THREAD
            &g_tensorrt_context,
#endif
            argc - cfg.channel_num - 1, argp))
        {
            fprintf(stderr, "Error parsing commandline arguments\n");
            return -1;
        }
        ctx[iterator].in_file_path = cfg.in_file_path[iterator];
        ctx[iterator].nvosd_context = nvosd_create_context();
        ctx[iterator].dec = NvVideoDecoder::createVideoDecoder(decname);
        TEST_ERROR(!ctx[iterator].dec, "Could not create decoder", cleanup);
        NVTX_RANGE_POP; // End create decoder

        NVTX_RANGE_PUSH("Init video decoder",2);
        // Subscribe to Resolution change event
        ret = ctx[iterator].dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE,
                        0, 0);
        TEST_ERROR(ret < 0,
                "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE",
                cleanup);

        // Set V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT control to false
        // so that application can send chunks/slice of encoded data instead of
        // forming complete frames. This needs to be done before setting format
        // on the output plane.
        ret = ctx[iterator].dec->disableCompleteFrameInputBuffer();
        TEST_ERROR(ret < 0, "Error in disableCompleteFrameInputBuffer", cleanup);

        // Set format on the output plane
        ret = ctx[iterator].dec->setOutputPlaneFormat(
                    ctx[iterator].decoder_pixfmt, CHUNK_SIZE);
        TEST_ERROR(ret < 0, "Could not set output plane format", cleanup);

        // V4L2_CID_MPEG_VIDEO_DISABLE_DPB should be set after output plane
        // set format
        if (ctx[iterator].disable_dpb)
        {
            ret = ctx[iterator].dec->disableDPB();
            TEST_ERROR(ret < 0, "Error in disableDPB", cleanup);
        }

        // Query, Export and Map the output plane buffers so that we can read
        // encoded data into the buffers
        ret = ctx[iterator].dec->output_plane.setupPlane(
                V4L2_MEMORY_MMAP, 10, true, false);
        TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);
        NVTX_RANGE_POP; // End init decoder

        ctx[iterator].in_file = new ifstream(ctx[iterator].in_file_path);
        TEST_ERROR(!ctx[iterator].in_file->is_open(),
                "Error opening input file", cleanup);

        if (ctx[iterator].out_file_path)
        {
            ctx[iterator].out_file = new ofstream(ctx[iterator].out_file_path);
            TEST_ERROR(!ctx[iterator].out_file->is_open(),
                        "Error opening output file",
                        cleanup);
        }

        NVTX_RANGE_PUSH("Create video converter",2);
        pthread_create(&ctx[iterator].render_feed_handle, NULL,
                                render_thread, &ctx[iterator]);
        pthread_setname_np(ctx[iterator].render_feed_handle, "render_thread");

        // Create converter to convert from BL to PL for writing raw video
        // to file
        char convname[512];
        sprintf(convname, "conv%d", iterator);
        ctx[iterator].conv = NvVideoConverter::createVideoConverter(convname);
        TEST_ERROR(!ctx[iterator].conv, "Could not create video converter",
                cleanup);

        ctx[iterator].conv->output_plane.
            setDQThreadCallback(conv_output_dqbuf_thread_callback);
        ctx[iterator].conv->capture_plane.
            setDQThreadCallback(conv_capture_dqbuf_thread_callback);
        ret = ctx[iterator].dec->output_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in output plane stream on", cleanup);
        if (cfg.channel_num == 1)
        {
            ctx[iterator].window_width = disp_info.window_width;
            ctx[iterator].window_height = disp_info.window_height;
            ctx[iterator].window_x = 0;
            ctx[iterator].window_y = 0;
        }
        else
        {
            if (iterator == 0)
            {
                ctx[iterator].window_width = disp_info.window_width / 2;
                ctx[iterator].window_height = disp_info.window_height / 2;
                ctx[iterator].window_x = 0;
                ctx[iterator].window_y = 0;
            }
            else if (iterator == 1)
            {
                ctx[iterator].window_width = disp_info.window_width / 2;
                ctx[iterator].window_height = disp_info.window_height / 2;
                ctx[iterator].window_x = disp_info.window_width / 2;
                ctx[iterator].window_y = 0;
            }
            else if (iterator == 2)
            {
                ctx[iterator].window_width = disp_info.window_width / 2;
                ctx[iterator].window_height = disp_info.window_height / 2;
                ctx[iterator].window_x = 0;
                ctx[iterator].window_y = disp_info.window_height / 2;
            }
            else
            {
                ctx[iterator].window_width = disp_info.window_width / 2;
                ctx[iterator].window_height = disp_info.window_height / 2;
                ctx[iterator].window_x = disp_info.window_width / 2;
                ctx[iterator].window_y = disp_info.window_height / 2;
            }
        }
        if (ctx[iterator].cpu_occupation_option != PARSER)
        {
            pthread_create(&ctx[iterator].dec_capture_loop, NULL,
                                dec_capture_loop_fcn, &ctx[iterator]);
            pthread_setname_np(ctx[iterator].dec_capture_loop, "dec_capture_loop");
        }
        pthread_create(&ctx[iterator].dec_feed_handle, NULL,
                                dec_feed_loop_fcn, &ctx[iterator]);
        pthread_setname_np(ctx[iterator].dec_feed_handle, "dec_feed_loop");
        NVTX_RANGE_POP; // End create converter
    }

cleanup:
    for (iterator = 0; iterator < cfg.channel_num; iterator++)
    {
        sem_wait(&(ctx[iterator].dec_run_sem)); //we need wait to make sure decode get EOS

        NVTX_RANGE_PUSH("cleanup",1);
        //send stop command to render, and wait it get consumed
        ctx[iterator].stop_render = 1;
        pthread_cond_broadcast(&ctx[iterator].render_cond);
        pthread_join(ctx[iterator].render_feed_handle, NULL);
#ifdef ENABLE_TENSORRT_THREAD
        if (TENSORRT_Stop == 0)
        {
            TENSORRT_Stop = 1;
            pthread_cond_broadcast(&TENSORRT_cond);
            pthread_join(TENSORRT_Thread_handle, NULL);
        }
#endif
        ctx[iterator].conv->waitForIdle(-1);
        ctx[iterator].conv->capture_plane.stopDQThread();
        ctx[iterator].conv->output_plane.stopDQThread();
        pthread_join(ctx[iterator].dec_feed_handle, NULL);
        if (ctx[iterator].cpu_occupation_option != PARSER)
            pthread_join(ctx[iterator].dec_capture_loop, NULL);

        if (ctx[iterator].dec->isInError())
        {
            cerr << "Decoder is in error" << endl;
            error = 1;
        }

        if (ctx[iterator].got_error)
        {
            error = 1;
        }

        if (ctx[iterator].do_stat)
            do_statistic(&ctx[iterator]);

        sem_destroy(&(ctx[iterator].dec_run_sem));
#ifdef ENABLE_TENSORRT_THREAD
        sem_destroy(&(ctx[iterator].result_ready_sem));
        pthread_mutex_destroy(&ctx[iterator].osd_lock);
#endif
        // The decoder destructor does all the cleanup i.e set streamoff on output and capture planes,
        // unmap buffers, tell decoder to deallocate buffer (reqbufs ioctl with counnt = 0),
        // and finally call v4l2_close on the fd.
        delete ctx[iterator].dec;

        // Similarly, EglRenderer destructor does all the cleanup
        if (ctx->cpu_occupation_option == PARSER_DECODER_VIC_RENDER)
            delete ctx[iterator].renderer;
        delete ctx[iterator].in_file;
        delete ctx[iterator].out_file;
        delete ctx[iterator].conv_output_plane_buf_queue;
        delete ctx[iterator].render_buf_queue;
        if (!ctx[iterator].nvosd_context)
        {
            nvosd_destroy_context(ctx[iterator].nvosd_context);
            ctx[iterator].nvosd_context = NULL;
        }
#ifdef ENABLE_TENSORRT_THREAD
        delete ctx[iterator].osd_queue;
#endif

        if (ctx[iterator].do_stat)
        {
            for( iter = ctx[iterator].frame_info_map->begin();
                    iter != ctx[iterator].frame_info_map->end(); iter++)
            {
                delete iter->second;
            }
        }
        delete ctx[iterator].frame_info_map;

        if (error)
        {
            cout << "App run failed" << endl;
        }
        else
        {
            cout << "App run was successful" << endl;
        }
    }
#ifdef ENABLE_TENSORRT_THREAD
#if LAB_INSTRUCTOR_FORCE_CPU_TENSORRT_PREP
    g_tensorrt_context.destroyTensorRtContext(true);
#else
    g_tensorrt_context.destroyTensorRtContext();
#endif
#endif
    // Terminate EGL display connection
    if (egl_display)
    {
        if (!eglTerminate(egl_display))
        {
            cout<<"Error while terminate EGL display connection";
            return -1;
        }
    }

    NVTX_RANGE_POP; // End cleanup

    return -error;
}
