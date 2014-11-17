################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/example_UVC/uvc_ctrl.c \
../src/example_UVC/uvc_driver.c \
../src/example_UVC/uvc_isight.c \
../src/example_UVC/uvc_queue.c \
../src/example_UVC/uvc_status.c \
../src/example_UVC/uvc_video.c 

OBJS += \
./src/example_UVC/uvc_ctrl.o \
./src/example_UVC/uvc_driver.o \
./src/example_UVC/uvc_isight.o \
./src/example_UVC/uvc_queue.o \
./src/example_UVC/uvc_status.o \
./src/example_UVC/uvc_video.o 

C_DEPS += \
./src/example_UVC/uvc_ctrl.d \
./src/example_UVC/uvc_driver.d \
./src/example_UVC/uvc_isight.d \
./src/example_UVC/uvc_queue.d \
./src/example_UVC/uvc_status.d \
./src/example_UVC/uvc_video.d 


# Each subdirectory must supply rules for building sources it contributes
src/example_UVC/%.o: ../src/example_UVC/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -D__KERNEL__=1 -I/usr/src/linux-source-3.2.0/include -I/usr/src/linux-source-3.2.0/arch/x86/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


