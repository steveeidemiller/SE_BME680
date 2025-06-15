/**
 * @file  SE_BME680.h
 * @brief Extension of the Adafruit BME680 library to add temperature compensation, humidity compensation, a dew point calculation, and a simple IAQ (indoor air quality) calculation
 */

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

    // Temperature offset in degrees Celsius, added to the raw temperature reading and used to compensate humidity and dew point calculations
    float temperature_offset = -2.00F;

    // Array of compensated gas readings used to calculate gas_ceiling
    double gas_calibration_data[GAS_CALIBRATION_DATA_POINTS];

    // Index for the next entry in the gas calibration data array, which wraps around to zero when the end of the array is reached
    int gas_calibration_data_index = 0;

    // The average highest compensated gas reading, derived from values stord in gas_calibration_data[], used as the threshold for a "good" air quality reading
    double gas_ceiling = 0;

    // Range of compensated gas resistance values used for gas calibration, calculated as a percentage of the maximum value in gas_calibration_data[]
    double gas_calibration_range = 0;

    // Timer for gas calibration stages, used to track sensor stabilization
    unsigned long gas_calibration_timer = 0;

    // Current stage of gas calibration: 0 = initialization, 1 = burn-in, 2 = normal operation
    int gas_calibration_stage = 0;

    // Ignore any values lower than this for the purposes of calculating the gas ceiling
    uint32_t gas_resistance_limit_min = 50000;

    // Ignore any values higher than this for the purposes of calculating the gas ceiling, which is important if the sensor is started in a low air quality environment
    uint32_t gas_resistance_limit_max = 225000;

    // Stage 0: Initialization time in milliseconds (30 seconds). The gas resistance will not be stable yet, but ceiling tracking can start and a low accuracy IAQ can be calculated. Resistance values prior to this time are very unstable.
    int gas_calibration_init_time = 30*1000;

    // Stage 1: Burn-in time in milliseconds (5 minutes). After this time, the gas resistance is expected to be moderately stable and a more accurate IAQ can be calculated.
    int gas_calibration_burnin_time = 5*60*1000;

    // Stage 2: Time in milliseconds (30 minutes) after which the gas calibration data decays and the gas ceiling needs to be recalculated. This is to account for sensor drift and changes in the environment.
    int gas_calibration_decay_time = 30*60*1000;

    // Slope of the linear compensation of the logatihmic gas resistance by the present humidity (see references)
    double iaq_slope_factor = 0.03;

    // Sensor uptime measured in decay intervals, used to estimate IAQ accuracy based on how long the sensor has been running in the current environment
    int32_t sensor_uptime = 0;
  
    /*!
    *  @brief  Common initialization code for all constructors
    */
    void initialize();

    /*!
    *  @brief  Update gas calibration data with a new compensated gas reading, calculate the arithmetic mean of the gas calibration data, and update the gas ceiling value
    *  @param  compensated_gas
    *          The compensated gas resistance value to be added to the gas calibration data
    *  @param  replaceSmallest
    *          If true, replace the smallest value in the gas calibration data array with the new compensated gas reading; otherwise, just add the new reading to the array
    */
    void updateGasCalibration(double compensated_gas, bool replaceSmallest = false);

    /*!
    *  @brief  Calculate the Indoor Air Quality (IAQ) based on the compensated gas resistance and the ongoing average gas ceiling
    */
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

    // Indor Air Quality (0-100%, bad to good), assigned after calling performReading() or endReading()
    float IAQ = 50.0F; // Default to 50% (neutral air quality) while accuracy is 0, which is the default "unreliable" accuracy level before any readings are taken

    // Estimated accuracy of the current IAQ reading: 0 = unreliable, 1 = low accuracy, 2 = moderate accuracy, 3 = high accuracy, 4 = very high accuracy
    int IAQ_accuracy = 0;

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

    /*!
    *  @brief  Set temperature compensation in degrees Celsius
    *  @param  degreesC
    *          Temperature offset in degrees Celsius to be added to the raw temperature reading and used to compensate humidity and dew point calculations
    */
    void setTemperatureCompensation(float degreesC);

    /*!
    *  @brief  Set temperature compensation in degrees Fahrenheit
    *  @param  degreesF
    *          Temperature offset in degrees Fahrenheit to be added to the raw temperature reading and used to compensate humidity and dew point calculations
    */
    void setTemperatureCompensationF(float degreesF);

    /*!
    *  @brief  Perform a reading from the BME680 sensor
    *  @return True if the reading was successful, false otherwise
    */
    bool performReading();

    /*!
    *  @brief  Begin a reading from the BME680 sensor
    *  @return The time in milliseconds until the reading is expected to complete
    */
    uint32_t beginReading();

    /*!
    *  @brief  End a reading from the BME680 sensor and process the results
    *  @return True if the reading was successful, false otherwise
    */
    bool endReading();

    /*!
    *  @brief Performs a reading and returns the dew point
    *  @return Dew point in degrees Celsius
    */
    float readDewPoint(void);
    
    /*!
    *  @brief Performs a reading and returns the compensated temperature
    *  @return Compensated temperature in degrees Celsius
    */
    float readCompensatedTemperature(void);

    /*!
    *  @brief Performs a reading and returns the compensated humidity
    *  @return Compensated humidity in percentage (0-100)
    */
    float readCompensatedHumidity(void);

    /*!
    *  @brief Performs a reading and returns the Indoor Air Quality (IAQ)
    *  @return IAQ value (0-100%, where 0% is bad air quality and 100% is good air quality)
    */
    float readIAQ(void);

    /*!
    *  @brief Get the estimated accuracy of the IAQ reading
    *  @return 0 = unreliable, 1 = low accuracy, 2 = moderate accuracy, 3 = high accuracy, 4 = very high accuracy
    */
    int getIAQAccuracy(void);

    /*!
    *  @brief Get the current gas calibration stage
    *  @details The gas calibration stages are:
    *           0 = Initialization stage (first 30 seconds), where gas resistance is not stable yet and no gas calibration data is collected
    *           1 = Burn-in stage (first 5 minutes), where gas resistance is expected to be moderately stable and a low accuracy IAQ can be calculated
    *           2 = Normal operation stage (after first 5 minutes)
    */
    int getGasCalibrationStage(void);

    /*!
    *  @brief Set gas resistance compensation slope factor
    *  @param slopeFactor
    *         The slope factor for the linear compensation of the logarithmic gas resistance by the present humidity (default 0.03)
    */
    bool setGasCompensationSlopeFactor(double slopeFactor = 0.03);

    /*!
    *  @brief Set the lower and upper "high" gas resistance limits for gas calibration
    *  @param minLimit
    *         The minimum gas resistance limit (in ohms) for gas ceiling calibration. High readings below this threshold are rounded up to it.
    *  @param maxLimit
    *         The maximum gas resistance limit (in ohms) above which gas readings are ignored for gas ceiling calibration
    */
    bool setUpperGasResistanceLimits(uint32_t minLimit = 30000, uint32_t maxLimit = 225000);

    /*!
    *  @brief Set timings for gas calibration stages
    *  @param initTime
    *         The time in milliseconds for the initialization stage (default 30 seconds)
    *  @param burninTime
    *         The time in milliseconds for the burn-in stage (default 5 minutes)
    *  @param decayTime
    *         The time in milliseconds for the decay stage (default 30 minutes)
    * @return True if the timings were set successfully, false if the timings are invalid
    *         (e.g., if initTime is not less than burninTime, or burninTime is not less than decayTime)
    */
    bool setGasCalibrationTimings(int initTime = 30 * 1000, int burninTime = 5 * 60 * 1000, int decayTime = 30 * 60 * 1000);
};

#endif
