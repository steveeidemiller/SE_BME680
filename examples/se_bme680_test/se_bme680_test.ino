/**
 * @file  se_bme680_test.ino
 * @brief Example sketch for testing the SE_BME680 library
 */

 #include <SE_BME680.h>

//TODO: add support for other constructors, e.g. with I2C address, SPI, etc.
SE_BME680 bme;

const char* divider = "----------------------------------------------";

void setup()
{
  // Serial initialization
  Serial.begin(115200);
  while (!Serial);

  // Initialize the BME680 sensor
  if (!bme.begin())
  {
    Serial.println("ABORT: Could not find a valid BME680 sensor");
    while (1);
  }

  // Uncomment this line to set a temperature compensation, which is also used to compensate humidity
  //bme.setTemperatureCompensation(-2.00F); // Celsius offset as type "float"

  /*
  // Set up the sensor. These settings are the default ones, but you can uncomment this code block and adjust them as needed.
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C for 150 ms
  */
}

void loop()
{
  if (bme.performReading())
  {
    Serial.println(divider);
    Serial.print("Reading successful at:     ");
    Serial.println(millis());
    Serial.println(divider);

    Serial.print("Temperature (raw):         ");
    Serial.print(bme.temperature); Serial.print(" °C   ");
    Serial.print(bme.temperature * 9.0F / 5.0F + 32.0F); Serial.println(" °F");

    Serial.print("Temperature (compensated): ");
    Serial.print(bme.temperature_compensated); Serial.print(" °C   ");
    Serial.print(bme.temperature_compensated * 9.0F / 5.0F + 32.0F); Serial.println(" °F");

    Serial.print("Humidity (raw):            ");
    Serial.print(bme.humidity);
    Serial.println(" %");

    Serial.print("Humidity (compensated):    ");
    Serial.print(bme.humidity_compensated);
    Serial.println(" %");

    Serial.print("Pressure:                  ");
    Serial.print(bme.pressure / 100.0); // Convert to mbar
    Serial.println(" mbar");

    Serial.print("Dew point:                 ");
    Serial.print(bme.dew_point); Serial.print(" °C   ");
    Serial.print(bme.dew_point * 9.0F / 5.0F + 32.0F); Serial.println(" °F");

    Serial.print("Gas resistance:            ");
    Serial.print(bme.gas_resistance);
    Serial.println(" ohms");

    Serial.print("IAQ:                       ");
    Serial.print(bme.IAQ);
    Serial.println(" %");

    Serial.print("IAQ accuracy:              ");
    Serial.print(bme.IAQ_accuracy);
    Serial.println();
  
    Serial.print("Gas Calibration Stage:     ");
    Serial.print(bme.getGasCalibrationStage());
    Serial.println();
  }
  else
  {
    Serial.println("ERROR: performReading() failed");
  }

  Serial.println(divider);
  Serial.println();
  Serial.println();

  delay(1000);
}
