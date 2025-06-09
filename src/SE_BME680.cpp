#include <SE_BME680.h>

//
SE_BME680::SE_BME680(TwoWire *wire) : Adafruit_BME680(wire)
{
  initialize();
}

//
SE_BME680::SE_BME680(int8_t cspin, SPIClass *spi) : Adafruit_BME680(cspin, spi)
{
  initialize();
}

//
SE_BME680::SE_BME680(int8_t cspin, int8_t mosipin, int8_t misopin, int8_t sckpin) : Adafruit_BME680(cspin, mosipin, misopin, sckpin)
{
  initialize();
}

//
void SE_BME680::initialize(void)
{
  // Initialize gas tracking array to zeros
  memset(gas_calibration_data, 0, sizeof(gas_calibration_data));

  //
  gas_calibration_timer = millis();
}

//
void SE_BME680::updateGasCalibration(double compensated_gas)
{
  // Add the compensated gas reading to the gas calibration data array
  gas_calibration_data[gas_calibration_data_index] = compensated_gas;
  gas_calibration_data_index++;
  if (gas_calibration_data_index >= GAS_CALIBRATION_DATA_POINTS)
  {
    gas_calibration_data_index = 0; // Wrap around to the beginning of the array
  }

  // Calculate the arithmetic mean of the calibration array (which may not be completely populated yet)
  double sum = 0;
  int j = 0;
  for (int i = 0; i < GAS_CALIBRATION_DATA_POINTS; i++)
  {
    if (gas_calibration_data[i] > 0) // Skip zero entries (which happen before the array is fully populated)
    {
      sum += gas_calibration_data[i];
      j++;
    }
  }
  if (j)
  {
    double mean = sum / (double)j;
    if (!isnan(mean))
    {
      // Update the gas ceiling value with the new mean
      gas_ceiling = mean;
    }
  }
}

//
// References:
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
  uint32_t resistance = max(gas_resistance, gas_resistance_limit_min); // Ensure resistance is within the defined limits
  double compensated_gas_r = (double)resistance * exp(iaq_slope_factor * hum_abs);
  if (isnan(compensated_gas_r)) return;

  /*
  //
  double calMin = 0, calMax = 0, calRange = 0;
  for (int i = 0; i < GAS_CALIBRATION_DATA_POINTS; i++)
  {
    if (gas_calibration_data[i] > 0)
    {
      if (calMin == 0) calMin = gas_calibration_data[i]; else calMin = min(calMin, gas_calibration_data[i]);
      if (calMax == 0) calMax = gas_calibration_data[i]; else calMax = max(calMax, gas_calibration_data[i]);
    }
  }
  if (calMax > 0) calRange = (calMax - calMin) / calMax * 100.0;

  //TODO: use range to determine IAQ readiness
  */

  //TODO: use a clock for init cycles, set accuracy to 0 while clock is < init cycles
  //TODO: continue the clock for about 5m for burn-in cycles, then set accuracy to 1
  //TODO: after burn-in cycles, set accuracy to 2 if gas comp range < 5%
  //TODO: after burn-in cycles, set accuracy to 3 if gas comp range < 2%

  switch (gas_calibration_stage)
  {
    case 0: // Initialization stage
      if (millis() - gas_calibration_timer >= gas_calibration_init_time)
      {
        gas_calibration_stage = 1; // Move to burn-in stage
      }
      break;

    case 1: // Burn-in stage
      if (millis() - gas_calibration_timer < gas_calibration_burnin_time)
      {
        updateGasCalibration(compensated_gas_r); // Collect gas calibration data
      }
      else
      {
        gas_calibration_stage = 2; // Move to normal operation stage
      }
      break;

    case 2: // Normal operation stage
      if (compensated_gas_r > gas_ceiling)
      {
        updateGasCalibration(compensated_gas_r); // Adapt ongoing average gas ceiling based on new high readings
      }
      else if (millis() - gas_calibration_timer >= gas_calibration_decay_time)
      {
        updateGasCalibration(compensated_gas_r); // Adapt ongoing average gas ceiling based on decay timings
        gas_calibration_timer = millis(); // Reset the calibration timer to start a new decay period
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
}

//
void SE_BME680::setTemperatureCompensation(float degreesC)
{
  temperature_offset = degreesC;
}

//
void SE_BME680::setTemperatureCompensationF(float degreesF)
{
  setTemperatureCompensation(degreesF * 5.0F / 9.0F); // Convert Fahrenheit to Celsius
}

//
bool SE_BME680::performReading(void)
{
  return endReading();
}

//
uint32_t SE_BME680::beginReading(void)
{
  // Proxy to base class
  return Adafruit_BME680::beginReading();
}

//
bool SE_BME680::endReading(void)
{
    // Proxy to base class
    if (!Adafruit_BME680::endReading())
    {
        return false;
    }

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
