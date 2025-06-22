/**
 * @file  DonchianAverage.h
 * @brief Helper class to smooth sensor measurements using a circular buffer to track an average value based on the min/max range over a specified number of samples.
 *        This is useful for removing oscillations in sensor readings due to the cycling of air conditioners, heaters, etc.
 * @link  https://www.investopedia.com/terms/d/donchianchannels.asp
 */

#ifndef __DONCHIAN_AVERAGE_H__
#define __DONCHIAN_AVERAGE_H__

class DonchianAverage
{
  private:
    float* data; // Array of data points
    int dataSize = 0; // Number of data points
    int cursor = 0; // Current index into the data array
    bool dataFull = false; // Set to true when the cursor wraps around and the array is full of data

  public:
    float current; // Current value for the metric, which is also at data[cursor]
    float min, max, average; // Statistics about the data in the array, updated each time new data points are added

    // Constructor
    DonchianAverage(int dataArraySize)
    {
      // Allocate memory for data array
      data = new float[dataArraySize];
      dataSize = dataArraySize;
      dataFull = false;
      cursor = 0;
    }

    // Destructor
    ~DonchianAverage()
    {
      // Free memory
      delete[] data;
      data = nullptr;
      dataSize = 0;
    }

    // Track a new data point and recompute min/max/average values
    void track(float dataPoint)
    {
      // Capture the current value
      current = dataPoint;

      // Add a new data point to the tracking array
      data[cursor] = dataPoint;
      cursor++;
      if (cursor >= dataSize)
      {
        cursor = 0; // Wrap around
        dataFull = true;
      }

      // Compute min/max/average values from the data array
      int j = dataFull ? dataSize : cursor;
      min = data[0];
      max = data[0];
      for (int i = 1; i < j; i++)
      {
        float d = data[i];
        if (d < min) min = d;
        if (d > max) max = d;
      }
      average = (min + max) / 2.0F; // Donchian Average is the midpoint between min and max
    }
};

#endif
