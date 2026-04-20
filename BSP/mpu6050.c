#include "mpu6050.h"
#include "stdio.h"
#include "string.h"
#include "MahonyAHRS.h"

MPU6050_Data_t data;
MPU6050_RawData_t Raw;

static void MPU6050_WriteReg(uint8_t RegAddr, uint8_t Data)
{
    HAL_I2C_Mem_Write(MPU6050_I2C, MPU6050_ADDR, RegAddr, I2C_MEMADD_SIZE_8BIT, &Data, 1, 1000);
}

static uint8_t MPU6050_ReadReg(uint8_t RegAddr)
{
    uint8_t Data;
   HAL_I2C_Mem_Read(MPU6050_I2C, MPU6050_ADDR, RegAddr, I2C_MEMADD_SIZE_8BIT, &Data, 1, 1000);
    return Data;
}

void MPU6050_Init(void)
{
	uint8_t data=0;
HAL_I2C_Mem_Read(&hi2c1,MPU6050_ADDR,MPU6050_WHO_AM_I,1,&data,1,1000);
	printf("data:%x",data);
    HAL_Delay(150);
    MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x00);    //复位
    
    MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);   //关闭睡眠模式 
    
    MPU6050_WriteReg(MPU6050_CONFIG, 0x03);
    MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 4);    //设置采样率为200Hz
    MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x18);    //设置陀螺仪传感器为±2000°/s
    MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x00);   //设置加速度传感器为±2g
}

void MPU6050_GetRawData(MPU6050_RawData_t *Raw)
{
    Raw->Accel_X = (int16_t)(MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H) << 8 |   MPU6050_ReadReg(MPU6050_ACCEL_XOUT_L)); //读取X轴加速度
    Raw->Accel_Y = (int16_t)(MPU6050_ReadReg(MPU6050_ACCEL_YOUT_H) << 8 |   MPU6050_ReadReg(MPU6050_ACCEL_YOUT_L)); //读取Y轴加速度
    Raw->Accel_Z = (int16_t)(MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_H) << 8 |   MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_L)); //读取Z轴加速度

    Raw->Temperature = (int16_t)(MPU6050_ReadReg(MPU6050_TEMP_OUT_H) << 8 | MPU6050_ReadReg(MPU6050_TEMP_OUT_L)); //读取温度
    Raw->Gyro_X = (int16_t)(MPU6050_ReadReg(MPU6050_GYRO_XOUT_H) << 8 |   MPU6050_ReadReg(MPU6050_GYRO_XOUT_L)); //读取X轴加速度
    Raw->Gyro_Y = (int16_t)(MPU6050_ReadReg(MPU6050_GYRO_YOUT_H) << 8 |   MPU6050_ReadReg(MPU6050_GYRO_YOUT_L)); //读取Y轴加速度
    Raw->Gyro_Z = (int16_t)(MPU6050_ReadReg(MPU6050_GYRO_ZOUT_H) << 8 |   MPU6050_ReadReg(MPU6050_GYRO_ZOUT_L));

}

void MPU6050_Update(MPU6050_Data_t *Data)
{
    MPU6050_GetRawData(&Raw);
    Data->Ax = (float)Raw.Accel_X / 16384.0f;
    Data->Ay = (float)Raw.Accel_Y / 16384.0f;
    Data->Az = (float)Raw.Accel_Z / 16384.0f;

    Data->T = (float)((Raw.Temperature / 340) + 36.53); //温度转换

    Data->Gx = (float)Raw.Gyro_X / 16.4f * 0.01745329f;
    Data->Gy = (float)Raw.Gyro_Y / 16.4f * 0.01745329f;
    Data->Gz = (float)Raw.Gyro_Z / 16.4f * 0.01745329f;
}

 float roll, pitch, yaw;
void Get_All_Value(void)
{
// 1. 采集IMU和编码器数据
		MPU6050_Update(&data);
    // 2. 姿态解算
		MahonyAHRSupdateIMU(data.Gx, data.Gy, data.Gz, data.Ax, data.Ay, data.Az);
    GetEulerAnglesDeg(&roll, &pitch, &yaw,data.Gz);
		printf("%.2f,%.2f,%.2f\r\n",roll,pitch,yaw);
	HAL_Delay(100);
}
