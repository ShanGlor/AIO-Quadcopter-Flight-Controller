#include <Math.h>
#include <Wire.h>
#include "imu.h"

   //B00000000 - 0
   //B00001000 - 1
   //B00010000 - 2
   //B00011000 - 3


static axis_float_t angle; // angle calculated using accelerometer
static axis_float_t gyroAngles;
static axis_int16_t gyroRates;

float roll, pitch;
float AcXRaw,AcYRaw,AcZRaw,TmpRaw,GyXRaw,GyYRaw,GyZRaw;

int delta_t;
long previousTime = 0;
long currentTime = 0;

#ifdef HORIZON
   median_filter_t accel_x_filter = median_filter_new(FILTER_COMPARISONS,0); //declare median filter for x axis 
   median_filter_t accel_y_filter = median_filter_new(FILTER_COMPARISONS,0); //declare median filter for y axis
   median_filter_t accel_z_filter = median_filter_new(FILTER_COMPARISONS,0); //declare median filter for z axis
#endif

 void initIMU(){ 
   Wire.begin();

   #ifdef I2C_FASTMODE
     Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
   #endif
   
   Wire.beginTransmission(MPU_ADDR);
       Wire.write(0x6B);  // PWR_MGMT_1 register
       Wire.write(0);     // set to zero (wakes up the MPU-6050)
   Wire.endTransmission(true);  
   Wire.beginTransmission(MPU_ADDR);
     Wire.write(0x1B);  // Access register 1B - gyroscope config

     #ifdef GYRO_SENSITIVITY_250
       Wire.write(B00000000); // Setting the gyro to full scale +/- 250 deg/sec
     #endif
     
     #ifdef GYRO_SENSITIVITY_500
       Wire.write(B00001000); // Setting the gyro to full scale +/- 500 deg/sec
     #endif

     #ifdef GYRO_SENSITIVITY_1000
       Wire.write(B00010000); // Setting the gyro to full scale +/- 1000 deg/sec
     #endif

      #ifdef GYRO_SENSITIVITY_2000
         Wire.write(B00011000); // Setting the gyro to full scale +/- 2000 deg/sec
      #endif
      
   Wire.endTransmission(true);
   Wire.beginTransmission(MPU_ADDR);
   Wire.write(0x1C);  // Access register 1C - accelerometer config

     #ifdef ACC_SENSITIVITY_2G
       Wire.write(B00000000); // Setting the accelerometer to +/- 2g
       #define ACCEL_SENS 16384
     #endif

     #ifdef ACC_SENSITIVITY_4G
       Wire.write(B00001000); // Setting the accelerometer to +/- 4g
       #define ACCEL_SENS 8192
     #endif

     #ifdef ACC_SENSITIVITY_8G
       Wire.write(B00010000); // Setting the accelerometer to +/- 8g
       #define ACCEL_SENS 4096
     #endif

     #ifdef ACC_SENSITIVITY_16G
       Wire.write(B00011000); // Setting the accelerometer to +/- 16g
       #define ACCEL_SENS 2048
     #endif

     
   Wire.endTransmission(true);
   Wire.beginTransmission(MPU_ADDR);
   Wire.write(0x1A);  // digital low pass filter register 0x1A

     #ifdef DIGITAL_LOW_PASS_FILTER
       Wire.write(B00000100); // ENABLING LOW PASS FILTRATION
     #else
       Wire.write(B00000000);
     #endif     
     
   Wire.endTransmission(true);
   Wire.beginTransmission(MPU_ADDR);
   Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
   Wire.endTransmission(false);

 }

void readIMU(){     
   Wire.beginTransmission(MPU_ADDR);
   Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
   Wire.endTransmission(false);
   Wire.requestFrom(MPU_ADDR,14);  // request a total of 14 registers
   AcXRaw=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
   AcYRaw=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
   AcZRaw=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
   TmpRaw=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
   GyYRaw=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
   GyXRaw=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
   GyZRaw=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
   
   processGyro();  

}

void processGyro(){  
  /*
  float new_rate_x = (float)(GyXRaw - GYRO_X_OFFSET) / GYRO_SENS;
  float new_rate_y = (float)(GyYRaw - GYRO_Y_OFFSET) / GYRO_SENS;
  float new_rate_z = (float)(GyZRaw - GYRO_Z_OFFSET) / GYRO_SENS;

  Integration of gyro rates to get the angles for debugging only
  gyroAngles.x += new_rate_x * (delta_t / 1000000);
  gyroAngles.y += new_rate_y * (delta_t / 1000000);
  gyroAngles.z += new_rate_z * (delta_t / 1000000);

  gyroRates.y = gyroRates.y + (delta_t / 1000000) / ((delta_t / 1000000)) * (new_rate_y - gyroRates.y);
  gyroRates.x = gyroRates.x + (delta_t / 1000000) / ((delta_t / 1000000)) * (new_rate_x - gyroRates.x);
  gyroRates.z = gyroRates.z + (delta_t / 1000000) / ((delta_t / 1000000)) * (new_rate_z - gyroRates.z);
  */
  
  gyroRates.y = (GyYRaw - GYRO_Y_OFFSET);
  gyroRates.x = (GyXRaw - GYRO_X_OFFSET);
  gyroRates.z = (GyZRaw - GYRO_Z_OFFSET);

  #if defined(ACRO) || defined(AIR)
    //do nothing
  #else
    processAcc();
  #endif

  
}

void processAcc(){
    //filtering accelerometer noise using a median filter
 #ifdef HORIZON
    axis_float_t accel_filtered; // filtered accelerometer raw values
   
    median_filter_in(accel_x_filter, AcXRaw);
    median_filter_in(accel_y_filter, AcYRaw);
    median_filter_in(accel_z_filter, AcZRaw);
    //outputting filtered data
    accel_filtered.x = (float) (median_filter_out(accel_x_filter));
    accel_filtered.y = (float) (median_filter_out(accel_y_filter));
    accel_filtered.z = (float) (median_filter_out(accel_z_filter));

//   roll = (atan2(accel_filtered.x, accel_filtered.z)*180)/M_PI; // -180° --> 180°
//   pitch = (atan2(accel_filtered.y, accel_filtered.z)*180)/M_PI; // -180° --> 180°

    roll = (atan2(accel_filtered.x, sqrt(sq(accel_filtered.y)+sq(accel_filtered.z)))*180)/M_PI; 
    pitch = (atan2(accel_filtered.y, sqrt(sq(accel_filtered.x)+sq(accel_filtered.z)))*180)/M_PI; 
  
     imuCombine();
 #endif   
 
}


void imuCombine(){
 
   angle.y = GYRO_PART * (angle.y + ((gyroRates.x / GYRO_SENS) * (delta_t / 1000000))) + (1-GYRO_PART) * pitch; //complementary filter
   angle.x = GYRO_PART * (angle.x + ((gyroRates.y / GYRO_SENS) * (delta_t / 1000000))) + (1-GYRO_PART) * roll;
   angle.z = ((gyroRates.z / GYRO_SENS) * (delta_t / 1000000));
//   angle.z = (angle.z + (gyroRates.z * (delta_t / 1000000))); //causes directional lock according to the direction faced by the quadcopter during startup (basically a magnetometer w/out accurate North reference)

/*
   Serial.print("X: ");
   Serial.print(angle.x);
   Serial.print("    Y: ");
   Serial.print(angle.y);
   Serial.print("    Z: ");
   Serial.println(angle.z);
*/   

   delta_t = (currentTime - previousTime);      
   previousTime = currentTime;
   currentTime = micros();
}


axis_int16_t imu_rates() {
  return gyroRates;
}

axis_float_t imu_angles() {
  return angle;
}

