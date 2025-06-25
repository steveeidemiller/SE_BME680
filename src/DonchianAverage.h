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
    float rangeLimitMax = 0.0F; // Optional maximum limit for the min/max range. Lookback period for the calculation will auto-reduce to enforce this limit. Zero means no limit.

  public:
    float current; // Current value for the metric, which is also at data[cursor]
    float min, max, average; // Statistics about the data in the array, updated each time new data points are added

    // Constructor
    DonchianAverage(int dataArraySize, float rangeLimitMax = 0.0F)
    {
      // Allocate memory for data array
      data = new float[dataArraySize];
      dataSize = dataArraySize;
      dataFull = false;
      cursor = 0;
      this->rangeLimitMax = rangeLimitMax;
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
      int k = cursor - 1; // Most recent index in the data array
      if (k < 0) k = dataSize - 1; // Wrap around at the beginning of the array
      float min = max = data[k]; // Start with the most recent data point and a range of zero
      for (int i = 1; i < j; i++)
      {
        // Walk backwards through the data array
        k--;
        if (k < 0) k = dataSize - 1; // Wrap around

        // Track min and max values
        float d = data[k];
        if (d < min) min = d;
        if (d > max) max = d;

        // If (a range limit was specified AND the current range exceeds that limit)...
        if (rangeLimitMax != 0.0F && (max - min) > rangeLimitMax)
        {
          // Adjust min or max to enforce the specified range limit with respect to the breakout direction
          if (max - dataPoint < dataPoint - min) // If (breakout direction is UP)...
          {
            min = max - rangeLimitMax; // Breakout to the upside, so raise min
          }
          else
          {
            max = min + rangeLimitMax; // Breakout to the downside, so lower max
          }

          // Stop looping after range limit is exceeded. Note that this effectively reduces the lookback period for the min/max calculation.
          break;
        }
      }

      // Update the public min/max/average values
      this->min = min;
      this->max = max;
      this->average = (min + max) / 2.0F; // Donchian Average is the midpoint between min and max
    }
};

#endif
