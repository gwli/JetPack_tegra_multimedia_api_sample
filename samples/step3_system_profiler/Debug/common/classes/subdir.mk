################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
/home/nvidia/JetsonLab/samples/master/common/classes/NvApplicationProfiler.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvBuffer.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvElement.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvElementProfiler.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvJpegDecoder.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvJpegEncoder.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvLogging.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvUtils.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvV4l2Element.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvV4l2ElementPlane.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvVideoConverter.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvVideoDecoder.cpp \
/home/nvidia/JetsonLab/samples/master/common/classes/NvVideoEncoder.cpp 

OBJS += \
./common/classes/NvApplicationProfiler.o \
./common/classes/NvBuffer.o \
./common/classes/NvElement.o \
./common/classes/NvElementProfiler.o \
./common/classes/NvJpegDecoder.o \
./common/classes/NvJpegEncoder.o \
./common/classes/NvLogging.o \
./common/classes/NvUtils.o \
./common/classes/NvV4l2Element.o \
./common/classes/NvV4l2ElementPlane.o \
./common/classes/NvVideoConverter.o \
./common/classes/NvVideoDecoder.o \
./common/classes/NvVideoEncoder.o 

CPP_DEPS += \
./common/classes/NvApplicationProfiler.d \
./common/classes/NvBuffer.d \
./common/classes/NvElement.d \
./common/classes/NvElementProfiler.d \
./common/classes/NvJpegDecoder.d \
./common/classes/NvJpegEncoder.d \
./common/classes/NvLogging.d \
./common/classes/NvUtils.d \
./common/classes/NvV4l2Element.d \
./common/classes/NvV4l2ElementPlane.d \
./common/classes/NvVideoConverter.d \
./common/classes/NvVideoDecoder.d \
./common/classes/NvVideoEncoder.d 


# Each subdirectory must supply rules for building sources it contributes
common/classes/NvApplicationProfiler.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvApplicationProfiler.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvBuffer.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvBuffer.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvElement.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvElement.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvElementProfiler.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvElementProfiler.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvJpegDecoder.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvJpegDecoder.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvJpegEncoder.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvJpegEncoder.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvLogging.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvLogging.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvUtils.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvUtils.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvV4l2Element.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvV4l2Element.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvV4l2ElementPlane.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvV4l2ElementPlane.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvVideoConverter.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvVideoConverter.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvVideoDecoder.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvVideoDecoder.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

common/classes/NvVideoEncoder.o: /home/nvidia/JetsonLab/samples/master/common/classes/NvVideoEncoder.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common/classes" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


