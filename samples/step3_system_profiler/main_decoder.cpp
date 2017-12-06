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

#define CHUNK_SIZE 4000000
#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define NAL_UNIT_START_CODE 0x00000001
#define MIN_CHUNK_SIZE      50

#define IS_NAL_UNIT_START(buffer_ptr) (!buffer_ptr[0] && !buffer_ptr[1] && \
        !buffer_ptr[2] && (buffer_ptr[3] == 1))

#define IS_NAL_UNIT_START1(buffer_ptr) (!buffer_ptr[0] && !buffer_ptr[1] && \
        (buffer_ptr[2] == 1))

using namespace std;

#ifdef ENABLE_TENSORRT_THREAD
#define OSD_BUF_NUM 100
static int frame_num = 1;  //this is used to filter image feeding to TENSORRT
static int input_frame_num = 0;

#define INPUT_FRAMES_MAX 50

#endif

static uint64_t ts[CHANNEL_NUM];
static uint64_t time_scale[CHANNEL_NUM];

static int
read_decoder_input_nalu(ifstream * stream, NvBuffer * buffer,
        char *parse_buffer, streamsize parse_buffer_size)
{
    // Length is the size of the buffer in bytes
    char *buffer_ptr = (char *) buffer->planes[0].data;

    char *stream_ptr;
    bool nalu_found = false;

    streamsize bytes_read;
    streamsize stream_initial_pos = stream->tellg();

    stream->read(parse_buffer, parse_buffer_size);
    bytes_read = stream->gcount();

    if (bytes_read == 0
        || input_frame_num > INPUT_FRAMES_MAX
        )
    {
        return buffer->planes[0].bytesused = 0;
    }

    ++input_frame_num;

    // Find the first NAL unit in the buffer
    stream_ptr = parse_buffer;
    while ((stream_ptr - parse_buffer) < (bytes_read - 3))
    {
        nalu_found = IS_NAL_UNIT_START(stream_ptr) ||
                        IS_NAL_UNIT_START1(stream_ptr);
        if (nalu_found)
        {
            break;
        }
        stream_ptr++;
    }

    // Reached end of buffer but could not find NAL unit
    if (!nalu_found)
    {
        cerr << "Could not read NAL unit from file. File seems to be corrupted"
            << endl;
        return -1;
    }

    memcpy(buffer_ptr, stream_ptr, 4);
    buffer_ptr += 4;
    buffer->planes[0].bytesused = 4;
    stream_ptr += 4;

    // Copy bytes till the next NAL unit is found
    while ((stream_ptr - parse_buffer) < (bytes_read - 3))
    {
        if (IS_NAL_UNIT_START(stream_ptr) || IS_NAL_UNIT_START1(stream_ptr))
        {
            streamsize seekto = stream_initial_pos +
                    (stream_ptr - parse_buffer);
            if (stream->eof())
            {
                stream->clear();
            }
            stream->seekg(seekto, stream->beg);
            return 0;
        }
        *buffer_ptr = *stream_ptr;
        buffer_ptr++;
        stream_ptr++;
        buffer->planes[0].bytesused++;
    }

    // Reached end of buffer but could not find NAL unit
    cerr << "Could not read nal unit from file. File seems to be corrupted"
            << endl;
    return -1;
}

static int
read_decoder_input_chunk(ifstream * stream, NvBuffer * buffer)
{
    //length is the size of the buffer in bytes
    streamsize bytes_to_read = MIN(CHUNK_SIZE, buffer->planes[0].length);

    stream->read((char *) buffer->planes[0].data, bytes_to_read);
    // It is necessary to set bytesused properly, so that decoder knows how
    // many bytes in the buffer are valid
    buffer->planes[0].bytesused = stream->gcount();
    return 0;
}

int
init_decode_ts()
{
    for (uint32_t i = 0; i < CHANNEL_NUM; i++)
    {
        ts[i] = 0L;
        time_scale[i] = 33000 * 10;
    }

    return 1;
}

static int
assign_decode_ts(struct v4l2_buffer *v4l2_buf, uint32_t channel)
{
    v4l2_buf->timestamp.tv_sec = ts[channel] + time_scale[channel];
    ts[channel] += time_scale[channel];

    return 1;
}

static nal_type_e
parse_nalu_unit(NvBuffer * buffer)
{
    unsigned char *pbuf = buffer->planes[0].data;

    return (nal_type_e)(*(pbuf + 4) & 0x1F);
}

static int
wait_for_nextFrame(context_t * ctx)
{
    if (ctx->cpu_occupation_option == PARSER_DECODER_VIC_RENDER)
        return 1;

    pthread_mutex_lock(&ctx->fps_lock);
    uint64_t decode_time_usec;
    uint64_t decode_time_sec;
    uint64_t decode_time_nsec;
    struct timespec last_decode_time;
    struct timeval now;
    gettimeofday(&now, NULL);

    last_decode_time.tv_sec = now.tv_sec;
    last_decode_time.tv_nsec = now.tv_usec * 1000L;

    decode_time_usec = 1000000L / ctx->fps;
    decode_time_sec = decode_time_usec / 1000000;
    decode_time_nsec = (decode_time_usec % 1000000) * 1000L;

    last_decode_time.tv_sec += decode_time_sec;
    last_decode_time.tv_nsec += decode_time_nsec;
    last_decode_time.tv_sec += last_decode_time.tv_nsec / 1000000000UL;
    last_decode_time.tv_nsec %= 1000000000UL;

    pthread_cond_timedwait(&ctx->fps_cond, &ctx->fps_lock,
                &last_decode_time);
    pthread_mutex_unlock(&ctx->fps_lock);

    return 1;
}

static void
query_and_set_capture(context_t * ctx)
{
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_format format;
    struct v4l2_crop crop;
    int32_t min_dec_capture_buffers;
    int ret = 0;
    int error = 0;
    uint32_t window_width;
    uint32_t window_height;
    char OSDcontent[512];

    // Get capture plane format from the decoder. This may change after
    // an resolution change event
    ret = dec->capture_plane.getFormat(format);
    TEST_ERROR(ret < 0,
            "Error: Could not get format from decoder capture plane", error);

    // Get the display resolution from the decoder
    ret = dec->capture_plane.getCrop(crop);
    TEST_ERROR(ret < 0,
           "Error: Could not get crop from decoder capture plane", error);

    if (ctx->cpu_occupation_option == PARSER_DECODER_VIC_RENDER)
    {
        // Destroy the old instance of renderer as resolution might changed
        delete ctx->renderer;

        if (ctx->fullscreen)
        {
            // Required for fullscreen
            window_width = window_height = 0;
        }
        else if (ctx->window_width && ctx->window_height)
        {
            // As specified by user on commandline
            window_width = ctx->window_width;
            window_height = ctx->window_height;
        }
        else
        {
            // Resolution got from the decoder
            window_width = crop.c.width;
            window_height = crop.c.height;
        }

        // If height or width are set to zero, EglRenderer creates a fullscreen
        // window
#ifdef RENDER_DIRECT
        bool threadedRenderer = false;
#else
        bool threadedRenderer = true;
#endif
        ctx->renderer =
            NvEglRenderer::createEglRenderer("renderer0", window_width,
                                           window_height, ctx->window_x,
                                           ctx->window_y, threadedRenderer);
        TEST_ERROR(!ctx->renderer,
                   "Error in setting up renderer. "
                   "Check if X is running or run with --disable-rendering",
                   error);

        ctx->renderer->setFPS(ctx->fps);
#ifndef ENABLE_TENSORRT_THREAD
        sprintf(OSDcontent, "Channel:%d", ctx->channel);
        ctx->renderer->setOverlayText(OSDcontent, 800, 50);
#endif
    }

    // deinitPlane unmaps the buffers and calls REQBUFS with count 0
    dec->capture_plane.deinitPlane();

    // Not necessary to call VIDIOC_S_FMT on decoder capture plane.
    // But decoder setCapturePlaneFormat function updates the class variables
    ret = dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat,
                                     format.fmt.pix_mp.width,
                                     format.fmt.pix_mp.height);
    TEST_ERROR(ret < 0, "Error in setting decoder capture plane format",
                error);

    // Get the minimum buffers which have to be requested on the capture plane
    ret =
        dec->getControl(V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
                        min_dec_capture_buffers);
    TEST_ERROR(ret < 0,
               "Error while getting value for V4L2_CID_MIN_BUFFERS_FOR_CAPTURE",
               error);

    // Request (min + 5) buffers, export and map buffers
    ret =
        dec->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                       min_dec_capture_buffers + 5, false,
                                       false);
    TEST_ERROR(ret < 0, "Error in decoder capture plane setup", error);

    // For file write, first deinitialize output and capture planes
    // of video converter and then use the new resolution from
    // decoder event resolution change
    ctx->conv->waitForIdle(1000);
    ctx->conv->output_plane.stopDQThread();
    ctx->conv->capture_plane.stopDQThread();

    ctx->conv->output_plane.deinitPlane();
    ctx->conv->capture_plane.deinitPlane();
    while(!ctx->conv_output_plane_buf_queue->empty())
    {
        ctx->conv_output_plane_buf_queue->pop();
    }


    ret = ctx->conv->setOutputPlaneFormat(format.fmt.pix_mp.pixelformat,
                                            format.fmt.pix_mp.width,
                                            format.fmt.pix_mp.height,
                                            V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
    TEST_ERROR(ret < 0, "Error in converter output plane set format",
                error);

    ret = ctx->conv->setCapturePlaneFormat(V4L2_PIX_FMT_ABGR32,//format.fmt.pix_mp.pixelformat,
                                            crop.c.width,
                                            crop.c.height,
                                            V4L2_NV_BUFFER_LAYOUT_PITCH);
    TEST_ERROR(ret < 0, "Error in converter capture plane set format",
                error);

    ret = ctx->conv->setCropRect(0, 0, crop.c.width, crop.c.height);
    TEST_ERROR(ret < 0, "Error while setting crop rect", error);

    ret =
        ctx->conv->output_plane.setupPlane(V4L2_MEMORY_DMABUF,
                                            dec->capture_plane.
                                            getNumBuffers(), false, false);
    TEST_ERROR(ret < 0, "Error in converter output plane setup", error);

    ret =
        ctx->conv->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                                 dec->capture_plane.
                                                 getNumBuffers(), true, false);
    TEST_ERROR(ret < 0, "Error in converter capture plane setup", error);
    ret = ctx->conv->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in converter output plane streamon", error);

    ret = ctx->conv->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in converter output plane streamoff",
                error);

    // Add all empty conv output plane buffers to conv_output_plane_buf_queue
    for (uint32_t i = 0; i < ctx->conv->output_plane.getNumBuffers(); i++)
    {
        ctx->conv_output_plane_buf_queue->push(ctx->conv->output_plane.
                getNthBuffer(i));
    }

    for (uint32_t i = 0; i < ctx->conv->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        ret = ctx->conv->capture_plane.qBuffer(v4l2_buf, NULL);
        TEST_ERROR(ret < 0, "Error Qing buffer at converter output plane",
                    error);
    }

    ctx->conv->output_plane.startDQThread(ctx);
    ctx->conv->capture_plane.startDQThread(ctx);
    // Capture plane STREAMON
    ret = dec->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in decoder capture plane streamon", error);

    // Enqueue all the empty capture plane buffers
    for (uint32_t i = 0; i < dec->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        ret = dec->capture_plane.qBuffer(v4l2_buf, NULL);
        TEST_ERROR(ret < 0, "Error Qing buffer at output plane", error);
    }
    cout << "Starting..." << endl;
    return;

error:
    if (error)
    {
        ctx->got_error = true;
        cerr << "Error in " << __func__ << endl;
    }
}

void *
dec_capture_loop_fcn(void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvVideoDecoder *dec = ctx->dec;
    map<uint64_t, frame_info_t*>::iterator  iter;
    struct v4l2_event ev;
    int ret;

    cout << "Starting decoder capture loop thread" << endl;
    // Need to wait for the first Resolution change event, so that
    // the decoder knows the stream resolution and can allocate appropriate
    // buffers when we call REQBUFS
    do
    {
        ret = dec->dqEvent(ev, 1000);
        if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                cerr <<
                    "Timed out waiting for first V4L2_EVENT_RESOLUTION_CHANGE"
                    << endl;
            }
            else
            {
                cerr << "Error in dequeueing decoder event" << endl;
            }
            ctx->got_error = true;
            break;
        }
    }
    while (ev.type != V4L2_EVENT_RESOLUTION_CHANGE);

    // query_and_set_capture acts on the resolution change event
    if (!ctx->got_error)
        query_and_set_capture(ctx);

    // Exit on error or EOS which is signalled in main()
    while (!(ctx->got_error || dec->isInError() || ctx->got_eos))
    {
        NvBuffer *dec_buffer;

        // Check for Resolution change again
        ret = dec->dqEvent(ev, false);
        if (ret == 0)
        {
            switch (ev.type)
            {
                case V4L2_EVENT_RESOLUTION_CHANGE:
                    query_and_set_capture(ctx);
                    continue;
            }
        }

        while (1)
        {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));
            v4l2_buf.m.planes = planes;

            // Dequeue a filled buffer
            if (dec->capture_plane.dqBuffer(v4l2_buf, &dec_buffer, NULL, 0))
            {
                if (errno == EAGAIN)
                {
                    usleep(1000);
                }
                else
                {
                    ctx->got_error = true;
                    cerr << "Error while calling dequeue at capture plane" <<
                        endl;
                }
                break;
            }

            if (ctx->do_stat)
            {
                iter = ctx->frame_info_map->find(v4l2_buf.timestamp.tv_sec);
                if (iter == ctx->frame_info_map->end())
                {
                    cout<<"image not return by decoder"<<endl;
                }
                else
                {
                    gettimeofday(&iter->second->output_time, NULL);
                }
            }

            if (ctx->cpu_occupation_option == PARSER_DECODER)
            {
                // Queue the buffer back once it has been used.
                if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0)
                {
                    ctx->got_error = true;
                    cerr <<
                        "Error while queueing buffer at decoder capture plane"
                        << endl;
                    break;
                }
                continue;
            }
            // If we need to write to file, give the buffer to video converter output plane
            // instead of returning the buffer back to decoder capture plane
            NvBuffer *conv_buffer;
            struct v4l2_buffer conv_output_buffer;
            struct v4l2_plane conv_planes[MAX_PLANES];

            memset(&conv_output_buffer, 0, sizeof(conv_output_buffer));
            memset(conv_planes, 0, sizeof(conv_planes));
            conv_output_buffer.m.planes = conv_planes;

            // Get an empty conv output plane buffer from conv_output_plane_buf_queue
            pthread_mutex_lock(&ctx->queue_lock);
            while (ctx->conv_output_plane_buf_queue->empty())
            {
                pthread_cond_wait(&ctx->queue_cond, &ctx->queue_lock);
            }
            conv_buffer = ctx->conv_output_plane_buf_queue->front();
            ctx->conv_output_plane_buf_queue->pop();
            pthread_mutex_unlock(&ctx->queue_lock);

            conv_output_buffer.index = conv_buffer->index;

            if (ctx->conv->output_plane.
                qBuffer(conv_output_buffer, dec_buffer) < 0)
            {
                ctx->got_error = true;
                cerr <<
                    "Error while queueing buffer at converter output plane"
                    << endl;
                break;
            }

        }
    }

    cout << "Exiting decoder capture loop thread" << endl;
    // Signal EOS to the decoder capture loop
    ctx->got_eos = true;

    //Signal VIC to wait EOS
    sem_post(&(ctx->dec_run_sem));
    return NULL;
}

void *
dec_feed_loop_fcn(void *arg)
{
    context_t *ctx = (context_t *) arg;
    int i = 0;
    bool eos = false;
    int ret;
    char *nalu_parse_buffer = NULL;
    nal_type_e nal_type;

    if (ctx->input_nalu)
    {
        nalu_parse_buffer = new char[CHUNK_SIZE];
    }

    // Read encoded data and enqueue all the output plane buffers.
    // Exit loop in case file read is complete.
    while (!eos && !ctx->got_error && !ctx->dec->isInError() &&
        i < (int)ctx->dec->output_plane.getNumBuffers())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        nvtxRangePush("Read video frame");

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        buffer = ctx->dec->output_plane.getNthBuffer(i);
        if (ctx->input_nalu)
        {
            read_decoder_input_nalu(ctx->in_file, buffer,
                    nalu_parse_buffer, CHUNK_SIZE);
            wait_for_nextFrame(ctx);
            if (ctx->cpu_occupation_option == PARSER)
            {
                if (buffer->planes[0].bytesused == 0)
                {
                    cout<<"Input file read complete"<<endl;
                    //Signal VIC to wait EOS
                    sem_post(&(ctx->dec_run_sem));
                    return NULL;
                }
                else
                    continue;
            }
        }
        else
        {
            read_decoder_input_chunk(ctx->in_file, buffer);
        }

        v4l2_buf.index = i;
        if (ctx->input_nalu && ctx->do_stat)
        {
            nal_type = parse_nalu_unit(buffer);
            switch (nal_type)
            {
                case NAL_UNIT_CODED_SLICE:
                case NAL_UNIT_CODED_SLICE_DATAPART_A:
                case NAL_UNIT_CODED_SLICE_DATAPART_B:
                case NAL_UNIT_CODED_SLICE_DATAPART_C:
                case NAL_UNIT_CODED_SLICE_IDR:
                {
                    assign_decode_ts(&v4l2_buf, ctx->channel);
                    frame_info_t *frame_meta = new frame_info_t;
                    memset(frame_meta, 0, sizeof(frame_info_t));

                    frame_meta->timestamp = v4l2_buf.timestamp.tv_sec;
                    gettimeofday(&frame_meta->input_time, NULL);
                    frame_meta->nal_type = nal_type;

                    ctx->frame_info_map->insert(
                        pair< uint64_t, frame_info_t* >(
                        v4l2_buf.timestamp.tv_sec, frame_meta));
                    break;
                }
                default:
                    break;
            }
        }

        nvtxRangePop();//("Read video frame");

        v4l2_buf.m.planes = planes;
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
        // It is necessary to queue an empty buffer to signal EOS to the decoder
        // i.e. set v4l2_buf.m.planes[0].bytesused = 0 and queue the buffer
        ret = ctx->dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            ctx->got_error = true;
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            eos = true;
            cout << "Input file read complete" << endl;
            break;
        }
        i++;
    }

    // Since all the output plane buffers have been queued, we first need to
    // dequeue a buffer from output plane before we can read new data into it
    // and queue it again.
    while (!eos && !ctx->got_error && !ctx->dec->isInError())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_buf.m.planes = planes;

        nvtxRangePushA("V4L2 NVDEC video decode");
        ret = ctx->dec->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, -1);
        nvtxRangePop();
        if (ret < 0)
        {
            cerr << "Error DQing buffer at output plane" << endl;
            ctx->got_error = true;
            break;
        }

        nvtxRangePush("Read video frame");

        if (ctx->input_nalu)
        {
            read_decoder_input_nalu(ctx->in_file, buffer,
                                    nalu_parse_buffer, CHUNK_SIZE);
            wait_for_nextFrame(ctx);
        }
        else
        {
            read_decoder_input_chunk(ctx->in_file, buffer);
        }

        if (ctx->input_nalu && ctx->do_stat)
        {
            nal_type = parse_nalu_unit(buffer);
            switch (nal_type)
            {
                case NAL_UNIT_CODED_SLICE:
                case NAL_UNIT_CODED_SLICE_DATAPART_A:
                case NAL_UNIT_CODED_SLICE_DATAPART_B:
                case NAL_UNIT_CODED_SLICE_DATAPART_C:
                case NAL_UNIT_CODED_SLICE_IDR:
                {
                    assign_decode_ts(&v4l2_buf, ctx->channel);
                    frame_info_t *frame_meta = new frame_info_t;
                    memset(frame_meta, 0, sizeof(frame_info_t));

                    frame_meta->timestamp = v4l2_buf.timestamp.tv_sec;
                    gettimeofday(&frame_meta->input_time, NULL);
                    frame_meta->nal_type = nal_type;
                    ctx->frame_info_map->insert(
                        pair< uint64_t, frame_info_t* >(
                        v4l2_buf.timestamp.tv_sec, frame_meta));
                    break;
                }
                default:
                    break;
            }
        }
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        nvtxRangePop();//("Read video frame");

        ret = ctx->dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            ctx->got_error = true;
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            eos = true;
            cout << "Input file read complete" << endl;
            break;
        }
    }

    // After sending EOS, all the buffers from output plane should be dequeued.
    // and after that capture plane loop should be signalled to stop.
    while (ctx->dec->output_plane.getNumQueuedBuffers() > 0 &&
           !ctx->got_error && !ctx->dec->isInError())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;
        nvtxRangePushA("V4L2 NVDEC video decode");
        ret = ctx->dec->output_plane.dqBuffer(v4l2_buf, NULL, NULL, -1);
        nvtxRangePop();
        if (ret < 0)
        {
            cerr << "Error DQing buffer at output plane" << endl;
            ctx->got_error = true;
            break;
        }
    }

    if (ctx->input_nalu)
    {
        delete []nalu_parse_buffer;
    }

    ctx->got_eos = true;
    return NULL;
}

