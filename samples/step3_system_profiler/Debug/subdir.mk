################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CU_SRCS += \
../CudaKernel.cu 

CPP_SRCS += \
../NvEglRenderer.cpp \
../main_decoder.cpp \
../main_graphics.cpp \
../main_tensorrt.cpp \
../v4l2_backend_main.cpp 

OBJS += \
./CudaKernel.o \
./NvEglRenderer.o \
./main_decoder.o \
./main_graphics.o \
./main_tensorrt.o \
./v4l2_backend_main.o 

CU_DEPS += \
./CudaKernel.d 

CPP_DEPS += \
./NvEglRenderer.d \
./main_decoder.d \
./main_graphics.d \
./main_tensorrt.d \
./v4l2_backend_main.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cu
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "." -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile --relocatable-device-code=false -gencode arch=compute_50,code=compute_50 -gencode arch=compute_52,code=compute_52 -gencode arch=compute_53,code=compute_53 -gencode arch=compute_60,code=compute_60 -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x cu -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "." -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


