

#ifndef __SE_BME680_H__
#define __SE_BME680_H__

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_SPIDevice.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#define  GAS_CALIBRATION_DATA_POINTS 100

class SE_BME680 : public Adafruit_BME680
{
  private:
    float temperature_offset = -1.5F; // Temperature offset in degrees Celsius, added to the raw temperature reading and used to compensate humidity and dew point calculations

    double gas_calibration_data[GAS_CALIBRATION_DATA_POINTS]; // Array of highest compensated gas readings used to calculate gas_ceiling
    int    gas_calibration_data_index = 0; // Index for the next entry in the gas calibration data array, which wraps around to zero when the end of the array is reached
    double gas_ceiling = 0; // The average highest compensated gas reading, used as the threshold for a "good" air quality reading

    unsigned long gas_calibration_timer = 0; // Timer for gas calibration, used to track upper limits of gas resistance during sensor stabilization
    int gas_calibration_stage = 0; // Current stage of gas calibration: 0 = initialization, 1 = burn-in, 2 = normal operation
    int gas_calibration_init_time = 30*1000; // Stage 0: Initialization time in milliseconds (30 seconds). The gas resistance will not be stable yet, but ceiling tracking can start and a low accuracy IAQ can be calculated. Resistance values prior to this time are very unstable.
    int gas_calibration_burnin_time = 5*60*1000; // Stage 1: Burn-in time in milliseconds (5 minutes). After this time, the gas resistance is expected to be moderately stable and a more accurate IAQ can be calculated.
    int gas_calibration_decay_time = 30*60*1000; // Stage 2: Time in milliseconds (30 minutes) after which the gas calibration data decays and the gas ceiling needs to be recalculated. This is to account for sensor drift and changes in the environment.
    int gas_calibration_accuracy = 0; // Current accuracy of the IAQ reading: 0 = unreliable, 1 = low accuracy, 2 = moderate accuracy, 3 = high accuracy

    void initialize();

    void updateGasCalibration(double compensated_gas);

    void calculateIAQ();

  public:

    // Dew point (Celsius) based on temperature and humidity, assigned after calling performReading() or endReading().
    // Note that the dew point is the same regardless of whether raw or compensated temperature and humidity are used since both dew point calculation and humidity compensation use the same Magnus transformations.
    float dew_point;

    // Compensated temperature (Celsius), assigned after calling performReading() or endReading()
    float temperature_compensated;

    // Compensated humidity (RH %), assigned after calling performReading() or endReading()
    float humidity_compensated;

    // NOTE: This ends up being the same as the "raw" dew point value since both dew point calculation and humidity compensation use the same Magnus transformations
    // Dew point (Celsius) based on compensated temperature and humidity, assigned after calling performReading() or endReading()
    //float dew_point_compensated;

    // Ignore any values lower than this for the purposes of calculating the gas ceiling
    uint32_t gas_resistance_limit_min = 100000;

    // Ignore any values higher than this for the purposes of calculating the gas ceiling
    uint32_t gas_resistance_limit_max = 175000;

    //
    double iaq_slope_factor = 0.03;

    // Indor Air Quality (0-100%, bad to good), assigned after calling performReading() or endReading()
    float IAQ = 50.0F; // Default to 50% (neutral air quality) while accuracy is 0, which is the default "unreliable" accuracy level before any readings are taken

    /*!
    *  @brief  Initialize with I2C
    *  @param  *wire
    *          Optional Wire object
    */
    SE_BME680(TwoWire *wire = &Wire);

    /*!
    *  @brief  Initialize with hardware SPI
    *  @param  cspin
    *          SPI chip select pin
    *  @param  spi
    *          Optional SPI object
    */
    SE_BME680(int8_t cspin, SPIClass *spi = &SPI);

    /*!
    *  @brief  Initialize with software SPI (bit-bang)
    *  @param  cspin
    *          SPI chip select pin
    *  @param  mosipin
    *          SPI MOSI pin (Data from microcontroller to sensor)
    *  @param  misopin
    *          SPI MISO pin (Data to microcontroller from sensor)
    *  @param  sckpin
    *          SPI clock pin (Data clock from microcontroller to sensor)
    */
    SE_BME680(int8_t cspin, int8_t mosipin, int8_t misopin, int8_t sckpin);

    // Sets the temperature offset in degrees Celsius. Added to the raw temperature reading and also used to compensate the humidity and dew point calculations.
    void setTemperatureCompensation(float degreesC);

    // Sets the temperature offset in degrees Fahrenheit. Added to the raw temperature reading and also used to compensate the humidity and dew point calculations.
    void setTemperatureCompensationF(float degreesF);

    //
    bool performReading();

    //
    uint32_t beginReading();

    //
    bool endReading();
};

#endif
