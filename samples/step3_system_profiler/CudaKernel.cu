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

#include <cuda.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "cudaEGL.h"
#include "NvAnalysis.h"

#include "v4l2_backend_test.h"

#define BOX_W 32
#define BOX_H 32

__constant__ int sample_count = 4;
__constant__ int samples[] =
{
        0,0,
        1,0,
        0,1,
        1,1,
};

__global__ void
prepareForTensorRtKernelRGB(CUdeviceptr pDevPtr,
        int src_width, int src_height, int src_pitch,
        int dst_width, int dst_height,
        void* cuda_buf)
{
    float *pdata = (float *)cuda_buf;
    char *psrcdata = (char *)pDevPtr;
    int dst_row = blockIdx.y * blockDim.y + threadIdx.y;
    int dst_col = blockIdx.x * blockDim.x + threadIdx.x;

    if (dst_col < dst_width && dst_row < dst_height)
    {
        int src_row = dst_row*2;
        int src_col = dst_col*2;

        for (int channel = 0; channel < 3; channel++)
        {
            int accum = 0;
            for (int s = 0; s < sample_count; ++s)
            {
                int offX=samples[s*2+0];
                int offY=samples[s*2+1];
                accum += (int)*(psrcdata + (src_row+offX) * src_pitch + (src_col+offY) * 4 + (3 - 1 - channel));
            }

            pdata[dst_width * dst_height * channel + dst_row * dst_width + dst_col] = (1.0f/sample_count)*accum;
        }
    }
}

__global__ void
prepareForTensorRtKernelBGR(CUdeviceptr pDevPtr,
        int src_width, int src_height, int src_pitch,
        int dst_width, int dst_height,
        void* cuda_buf)
{
    float *pdata = (float *)cuda_buf;
    char *psrcdata = (char *)pDevPtr;
    int dst_row = blockIdx.y * blockDim.y + threadIdx.y;
    int dst_col = blockIdx.x * blockDim.x + threadIdx.x;

    // BGR offset for 3 classes
    int offsets[] = {124, 117, 104};

    if (dst_col < dst_width && dst_row < dst_height)
    {
        int src_row = dst_row*2;
        int src_col = dst_col*2;

        // For V4L2_PIX_FMT_ABGR32 --> BGRA-8-8-8-8
        for (int k = 0; k < 3; k++)
        {
            int accum = 0;
            for (int s = 0; s < sample_count; ++s)
            {
                int offX=samples[s*2+0];
                int offY=samples[s*2+1];
                accum += (int)*(psrcdata + (src_row+offX) * src_pitch + (src_col+offY) * 4 + k) - offsets[k];
            }

            pdata[dst_width * dst_height * k + dst_row * dst_width + dst_col] = (1.0f/sample_count)*accum;

        }
    }
}

int prepareForTensorRt(CUdeviceptr pDevPtr,
                      int src_width,
                      int src_height,
                      int src_pitch,
                      int dst_width,
                      int dst_height,
                      COLOR_FORMAT color_format,
                      void* cuda_buf)
{
    dim3 threadsPerBlock(32, 32);
    dim3 blocks(dst_width/threadsPerBlock.x, dst_height/threadsPerBlock.y);


    if (color_format == COLOR_FORMAT_RGB)
    {
        prepareForTensorRtKernelRGB<<<blocks, threadsPerBlock>>>(pDevPtr,
                src_width, src_height, src_pitch, dst_width, dst_height,
                cuda_buf);
    }
    else if (color_format == COLOR_FORMAT_BGR)
    {
        prepareForTensorRtKernelBGR<<<blocks, threadsPerBlock>>>(pDevPtr,
                src_width, src_height, src_pitch, dst_width, dst_height,
                cuda_buf);
    }

    return 0;
}

/**
  * Performs map egl image into cuda memory.
  *
  * @param pEGLImage: EGL image
  * @param width: Image width
  * @param height: Image height
  * @param color_format: The input color format
  * @param cuda_buf: destnation cuda address
  */
bool prepareEGLImage2FloatForTensorRt(void* pEGLImage, int width, int height, COLOR_FORMAT color_format, void* cuda_buf)
{
    CUresult status;
    CUeglFrame eglFrame;
    CUgraphicsResource pResource = NULL;
    EGLImageKHR *pImage = (EGLImageKHR *)pEGLImage;

    cudaFree(0);
    status = cuGraphicsEGLRegisterImage(&pResource, *pImage,
                CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsEGLRegisterImage failed: %d, cuda process stop\n",
                        status);
        return false;
    }

    status = cuGraphicsResourceGetMappedEglFrame(&eglFrame, pResource, 0, 0);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsSubResourceGetMappedArray failed\n");
        return false;
    }

    status = cuCtxSynchronize();
    if (status != CUDA_SUCCESS)
    {
        printf("cuCtxSynchronize failed\n");
        return false;
    }

    if (eglFrame.frameType == CU_EGL_FRAME_TYPE_PITCH)
    {
        CUdeviceptr eglSrcPtr = (CUdeviceptr) eglFrame.frame.pPitch[0];

        // Using GPU to convert int buffer into float buffer.
        prepareForTensorRt(
                          eglSrcPtr,
                          eglFrame.width,
                          eglFrame.height,
                          eglFrame.pitch,
                          width,
                          height,
                          color_format,
                          cuda_buf);
    }
    status = cuCtxSynchronize();
    if (status != CUDA_SUCCESS)
    {
        printf("cuCtxSynchronize failed after prepareForTensorRt\n");
        return false;
    }

    status = cuGraphicsUnregisterResource(pResource);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsEGLUnRegisterResource failed: %d\n", status);
        return false;
    }

    return true;
}
