/**
 * @file  SE_BME680.cpp
 * @brief Extension of the Adafruit BME680 library to add temperature compensation, humidity compensation, a dew point calculation, and a simple IAQ (indoor air quality) calculation
 */

#include <SE_BME680.h>

// SE_BME680 IAC constructor
SE_BME680::SE_BME680(TwoWire *wire) : Adafruit_BME680(wire)
{
  initialize();
}

// SE_BME680 SPI constructor
SE_BME680::SE_BME680(int8_t cspin, SPIClass *spi) : Adafruit_BME680(cspin, spi)
{
  initialize();
}

// SE_BME680 software SPI constructor
SE_BME680::SE_BME680(int8_t cspin, int8_t mosipin, int8_t misopin, int8_t sckpin) : Adafruit_BME680(cspin, mosipin, misopin, sckpin)
{
  initialize();
}

// Common initialization code for all constructors
void SE_BME680::initialize(void)
{
  // Reset globals
  memset(gas_calibration_data, 0, sizeof(gas_calibration_data)); // Initialize gas tracking array to zeros
  gas_calibration_stage = 0; // Default to initialization stage
  gas_calibration_data_index = 0; // Default to the first entry in the gas calibration data array
  gas_calibration_range = 0; // No data yet, so no range
  gas_ceiling = 0; // Default to zero gas ceiling
  IAQ = 50.0F; // Default to 50% (neutral air quality) while accuracy is 0, which is the default "unreliable" accuracy level before any readings are taken
  IAQ_accuracy = 0; // Default to unreliable accuracy
  sensor_uptime = 0; // Reset uptime tracking

  // Reset the gas calibration timer
  gas_calibration_timer = millis();
}

// Update gas calibration data with a new compensated gas reading, calculate the arithmetic mean of the gas calibration data, and update the gas ceiling value
void SE_BME680::updateGasCalibration(double compensated_gas, bool replaceSmallest)
{
  // Update the array of compensated gas readings with the new compensated gas reading
  if (replaceSmallest && gas_calibration_data[GAS_CALIBRATION_DATA_POINTS - 1] > 0) // If (replaceSmallest is true AND the array is already full of values collected during burn-in)...
  {
    // Replace the smallest value in the gas calibration data array with the new compensated gas reading
    double smallest_value = gas_calibration_data[0];
    int smallest_index = 0;
    for (int i = 1; i < GAS_CALIBRATION_DATA_POINTS; i++)
    {
      if (gas_calibration_data[i] < smallest_value)
      {
        smallest_value = gas_calibration_data[i];
        smallest_index = i;
      }
    }
    if (compensated_gas > smallest_value)
    {
      // Replace the smallest value with the new compensated gas reading
      gas_calibration_data[smallest_index] = compensated_gas;
    }
  }
  else
  {
    // Add the compensated gas reading to the gas calibration data array
    gas_calibration_data[gas_calibration_data_index] = compensated_gas;
    gas_calibration_data_index++;
    if (gas_calibration_data_index >= GAS_CALIBRATION_DATA_POINTS)
    {
      // Wrap around to the beginning of the array
      gas_calibration_data_index = 0;
    }
  }

  // Calculate the arithmetic mean and min/max range of the calibration array (which may not be completely populated yet)
  double dataPoint, sum = 0, mean = 0, calMin = 0, calMax = 0, calRange = 0;
  int count = 0;
  for (int i = 0; i < GAS_CALIBRATION_DATA_POINTS; i++)
  {
    dataPoint = gas_calibration_data[i];
    if (dataPoint > 0) // Skip zero entries (which happen before the array is fully populated)
    {
      sum += dataPoint;
      if (calMin == 0) calMin = dataPoint; else calMin = min(calMin, dataPoint);
      if (calMax == 0) calMax = dataPoint; else calMax = max(calMax, dataPoint);
      count++;
    }
  }
  if (count)
  {
    if (calMax > 0)
    {
      // Calculate the min/max range as a percentage of the maximum value        
      gas_calibration_range = (calMax - calMin) / calMax;
    }
    mean = sum / (double)count;
    if (!isnan(mean))
    {
      // Update the gas ceiling value with the new mean
      gas_ceiling = mean;
    }
  }
}

// Calculate the Indoor Air Quality (IAQ) based on the compensated gas resistance and the ongoing average gas ceiling
// References and credits for the IAQ calculation:
//   https://github.com/thstielow/raspi-bme680-iaq
//   https://forums.pimoroni.com/t/bme680-observed-gas-ohms-readings/6608/18
void SE_BME680::calculateIAQ()
{
  // Ignore spurious gas readings. Documented range is 50-50k ohms, typical. Note that ignoring high readings may increase stabilization time.
  if (gas_resistance > gas_resistance_limit_max)
  {
    if (gas_calibration_stage < 2)
    {
      // Add 1 second to the calibration timer to allow more time to stabilize
      gas_calibration_timer += 1000;
    }
    return;
  }

  // Calculate the saturation water vapor density of air at the current temperature (°C) in kg/m^3, which is equal to a relative humidity of 100% at the current temperature
  double svd = (6.112 * 100.0 * exp(17.625 * temperature / (243.04 + temperature))) / (461.52 * (temperature + 273.15));

  // Calculate absolute humidity using the saturation water density
  double hum_abs = humidity * 10 * svd;

  // Compensate exponential impact of humidity on resistance
  double factor = exp(iaq_slope_factor * hum_abs); // Exponential factor based on humidity
  double compensated_gas_r = (double)gas_resistance * factor; // Compensated gas resistance based on the humidity factor
  double compensated_gas_r_min = (double)gas_resistance_limit_min * factor; // Compensated minimum gas resistance limit based on the humidity factor, important if the sensor is started in a low air quality environment
  if (isnan(compensated_gas_r) || isnan(compensated_gas_r_min)) return;

  // Update gas calibration data with the compensated gas resistance value
  switch (gas_calibration_stage)
  {
    case 0: // Initialization stage. Gas readings are simply ignored until the sensor stabilizes.
      if (millis() - gas_calibration_timer >= gas_calibration_init_time)
      {
        gas_calibration_stage = 1; // Move to burn-in stage
      }
      break;

    case 1: // Burn-in stage. The sensor is expected to start mildly stabilizing and gas ceiling values can now be collected.
      if (millis() - gas_calibration_timer < gas_calibration_burnin_time)
      {
        // Fill the calibration array first, and then continue to update the array by replacing the smallest value. This effectively collects the highest witnessed compensated gas resistance values during burn-in.
        updateGasCalibration(max(compensated_gas_r, compensated_gas_r_min), true); // Limit calibration data to the compensated minimum gas resistance limit
      }
      else
      {
        gas_calibration_stage = 2; // Move to normal operation stage
      }
      break;

    case 2: // Normal operation stage
      if (compensated_gas_r > compensated_gas_r_min)
      {
        if (compensated_gas_r > gas_ceiling)
        {
          // Integrate new higher gas readings into the gas calibration data array to establish a better gas ceiling for "good" air quality
          updateGasCalibration(compensated_gas_r, false); // Adapt ongoing average gas ceiling based on new high readings
        }
        else if (millis() - gas_calibration_timer >= gas_calibration_decay_time)
        {
          // Rotate out older values from the gas calibration data array to account for sensor drift and changes in the environment
          updateGasCalibration(compensated_gas_r, false); // Adapt ongoing average gas ceiling based on decay timings
          gas_calibration_timer = millis(); // Reset the calibration timer to start a new decay period
          sensor_uptime++; // Increment sensor uptime to track how long the sensor has been running in the current environment
        }
      }
      break;
  }

  // Calculate IAQ based on compensated gas resistance and the ongoing average gas ceiling
  if (gas_ceiling)
  {
    // Calculate relative air quality on a scale of 0-100% using a quadratic ratio for steeper scaling at higher air qualities
    double quality = pow(compensated_gas_r / gas_ceiling, 2) * 100.0;
    IAQ = min((float)quality, 100.0F); // Ensure IAQ does not exceed 100%
  }

  // Estimate IAQ calculation accuracy based on gas calibration timing stage, calibration data range and sensor uptime
  switch (gas_calibration_stage)
  {
    case 0: // Initialization stage
      IAQ_accuracy = 0; // Unreliable
      break;
    case 1: // Burn-in stage
      IAQ_accuracy = 1; // Low accuracy
      break;
    case 2: // Normal operation stage
      IAQ_accuracy = 1; // Low accuracy by default
      if (gas_calibration_range < 0.075) IAQ_accuracy = 2; // Moderate accuracy
      if (gas_calibration_range < 0.035 && sensor_uptime >= 2) IAQ_accuracy = 3; // High accuracy, requires at least several decay intervals of sensor uptime in the current environment
      if (gas_calibration_range < 0.02 && sensor_uptime >= 100) IAQ_accuracy = 4; // Very high accuracy, which typicall requires days of sensor uptime in the current environment
      break;
  }
}

// Set temperature compensation in degrees Celsius
void SE_BME680::setTemperatureCompensation(float degreesC)
{
  temperature_offset = degreesC;
}

// Set temperature compensation in degrees Fahrenheit
void SE_BME680::setTemperatureCompensationF(float degreesF)
{
  setTemperatureCompensation(degreesF * 5.0F / 9.0F); // Convert Fahrenheit to Celsius
}

// Perform a reading from the BME680 sensor
bool SE_BME680::performReading(void)
{
  return endReading();
}

// Begin a reading from the BME680 sensor
uint32_t SE_BME680::beginReading(void)
{
  // Proxy to base class
  return Adafruit_BME680::beginReading();
}

// End a reading from the BME680 sensor
bool SE_BME680::endReading(void)
{
  // Proxy to base class
  if (!Adafruit_BME680::endReading()) return false;

  // Dew point calculation using raw measurements and the Magnus formula: https://en.wikipedia.org/wiki/Dew_point#Calculating_the_dew_point
  float magnusGammaTRH = (float)log(humidity / 100.0F) + 17.625F * temperature / (243.04F + temperature);
  dew_point = 243.04F * magnusGammaTRH / (17.625F - magnusGammaTRH); // Celsius

  // Compensate temperature based on the temperature offset
  temperature_compensated = temperature + temperature_offset; // Celsius

  // Compensate humidity based on the temperature offset
  float svpMeasured = 6.112F * exp(17.625F * temperature / (243.04F + temperature)); // Saturation vapor pressure at the measured temperature
  float avpMeasured = humidity / 100.0F * svpMeasured; //The actual vapor pressure represents the real amount of water vapor in the air. It can be calculated from the measured relative humidity and the saturation vapor pressure at the measured temperature.
  float svpCompensated = 6.112F * exp(17.625F * temperature_compensated / (243.04F + temperature_compensated)); // Saturation vapor pressure at the compensated temperature
  humidity_compensated = avpMeasured / svpCompensated * 100.0F; // Relative humidity at the compensated temperature

  // NOTE: This ends up being identical to the dew point value calculated above since both dew point calculation and humidity compensation use the same Magnus transformations
  // Compensated dew point calculation using compensated measurements and the Magnus formula: https://en.wikipedia.org/wiki/Dew_point#Calculating_the_dew_point
  //magnusGammaTRH = (float)log(humidity_compensated / 100.0F) + 17.625F * temperature_compensated / (243.04F + temperature_compensated);
  //dew_point_compensated = 243.04F * magnusGammaTRH / (17.625F - magnusGammaTRH); // Celsius

  // Calculate IAQ
  calculateIAQ();

  // Return true to indicate a successful reading
  return true;
}

// Perform a reading and return the dew point
float SE_BME680::readDewPoint(void)
{
  performReading();
  return dew_point;
}

// Perform a reading and return the compensated temperature
float SE_BME680::readCompensatedTemperature(void)
{
  performReading();
  return temperature_compensated;
}

// Perform a reading and return the compensated humidity
float SE_BME680::readCompensatedHumidity(void)
{
  performReading();
  return humidity_compensated;
}

// Perform a reading and return the Indoor Air Quality (IAQ)
float SE_BME680::readIAQ(void)
{
  performReading();
  return IAQ;
}

// Get IAQ accuracy
int SE_BME680::getIAQAccuracy(void)
{
  return IAQ_accuracy;
}

// Get the current gas calibration stage
int SE_BME680::getGasCalibrationStage(void)
{
  return gas_calibration_stage;
}

// Set gas resistance compensation slope factor
bool SE_BME680::setGasCompensationSlopeFactor(double slopeFactor)
{
//TODO: Validate slope factor range, e.g., 0.01 to 0.1
  iaq_slope_factor = slopeFactor;
  return true; // Slope factor successfully set
}

// Set the lower and upper "high" gas resistance limits for gas calibration
bool SE_BME680::setUpperGasResistanceLimits(uint32_t minLimit, uint32_t maxLimit)
{
  if (minLimit >= 30000 && maxLimit <= 2000000 && minLimit <= maxLimit)
  {
    // Set the limits only if they are within a reasonable range
    gas_resistance_limit_min = minLimit;
    gas_resistance_limit_max = maxLimit;
    return true; // Limits successfully set
  }
  return false; // Invalid limits
}

// Set timings for gas calibration stages
bool SE_BME680::setGasCalibrationTimings(int initTime, int burninTime, int decayTime)
{
  if (initTime > 0 && burninTime >= initTime && decayTime >= burninTime)
  {
    // Ensure that the timings are valid
    if (initTime < 1000) initTime = 1000; // Minimum 1 second for initialization
    if (burninTime < initTime + 1000) burninTime = initTime + 1000; // Minimum 1 second after initialization for burn-in
    if (decayTime < burninTime + 60000) decayTime = burninTime + 60000; // Minimum 1 minute after burn-in for decay
    gas_calibration_init_time = initTime;
    gas_calibration_burnin_time = burninTime;
    gas_calibration_decay_time = decayTime;
    return true; // Timings successfully set
  }
  return false; // Invalid timings
}
