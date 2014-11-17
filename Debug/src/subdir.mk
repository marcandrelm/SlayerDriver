################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/CamDriver.c \
../src/CharDriver.c \
../src/USB-Skeleton.c 

OBJS += \
./src/CamDriver.o \
./src/CharDriver.o \
./src/USB-Skeleton.o 

C_DEPS += \
./src/CamDriver.d \
./src/CharDriver.d \
./src/USB-Skeleton.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -D__KERNEL__=1 -I/usr/src/linux-source-3.2.0/include -I/usr/src/linux-source-3.2.0/arch/x86/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


