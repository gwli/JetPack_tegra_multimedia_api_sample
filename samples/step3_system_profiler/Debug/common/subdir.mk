################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
/home/nvidia/JetsonLab/samples/master/common/v4l2_backend_csvparser.cpp 

OBJS += \
./common/v4l2_backend_csvparser.o 

CPP_DEPS += \
./common/v4l2_backend_csvparser.d 


# Each subdirectory must supply rules for building sources it contributes
common/v4l2_backend_csvparser.o: /home/nvidia/JetsonLab/samples/master/common/v4l2_backend_csvparser.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: NVCC Compiler'
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 -ccbin aarch64-linux-gnu-g++ -gencode arch=compute_50,code=sm_50 -gencode arch=compute_52,code=sm_52 -gencode arch=compute_53,code=sm_53 -gencode arch=compute_60,code=sm_60 -m64 -odir "common" -M -o "$(@:%.o=%.d)" "$<"
	/usr/local/cuda-8.0/bin/nvcc -DENABLE_TENSORRT_THREAD -DENABLE_TENSORRT -I"/home/nvidia/JetsonLab/samples/master/include" -I"/home/nvidia/JetsonLab/samples/master/include/libjpeg-8b" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/cuda" -I"/home/nvidia/JetsonLab/samples/master/common/algorithm/tensorrt" -G -g -O0 -std=c++11 --compile -m64 -ccbin aarch64-linux-gnu-g++ -Wno-deprecated-gpu-targets   -x c++ -o  "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


