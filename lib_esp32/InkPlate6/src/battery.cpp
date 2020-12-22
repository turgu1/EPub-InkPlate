#define __BATTERY__ 1
#include "battery.hpp"

#include "wire.hpp"

Battery Battery::singleton;

bool
Battery::setup()
{
  mcp.set_direction(BATTERY_SWITCH, MCP::PinMode::OUTPUT);

  return true;
}

double 
Battery::read_level()
{
  Wire::enter();
  mcp.digital_write(BATTERY_SWITCH, MCP::SignalLevel::HIGH);
  Wire::leave();

  ESP::delay(1);
  
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);

  int16_t adc = ESP::analog_read(ADC1_CHANNEL_7); // ADC 1 Channel 7 is GPIO port 35
  
  Wire::enter();
  mcp.digital_write(BATTERY_SWITCH, MCP::SignalLevel::LOW);
  Wire::leave();

  return (double(adc) * 3.3 * 2) / 4095.0;
}
