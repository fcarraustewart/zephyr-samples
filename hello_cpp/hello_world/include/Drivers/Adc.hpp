#pragma once

class Adc {
public:
	// Initialize the ADC
	static int InitializeDriver();

	// Read multiple channels
	static int ReadAllChannels();

};
