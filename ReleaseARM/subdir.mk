################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../level_bed.cc \
../machine.cc \
../main.cc \
../mesh_builder.cc \
../serial.cc \
../tui.cc \
../utility.cc 

CC_DEPS += \
./level_bed.d \
./machine.d \
./main.d \
./mesh_builder.d \
./serial.d \
./tui.d \
./utility.d 

OBJS += \
./level_bed.o \
./machine.o \
./main.o \
./mesh_builder.o \
./serial.o \
./tui.o \
./utility.o 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabi-g++ -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


