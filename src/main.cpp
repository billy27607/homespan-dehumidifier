/*
	This is a homekit appliance which allos Apple home to control an Alorair HDi65 dehumidifier that
	I bought to reduce the humidity in a vacation home. It is based on the wonderful library HomeSpan
	and I have it running on a xiao esp32c3. The humidifier does all the work of trying to hold to a target
	humidity. this is just a homekit control for it.

*/
#include <AlorairHDi65.h>
#include "HomeSpan.h"
#include <esp_mac.h>
#include "config.h"
#include "WebPortal.h"

// instantiate the HDi65

AlorairHDi65 alorair(D7);

// Dehumidifier structure

struct HumidifierDehumidifier : Service::HumidifierDehumidifier
{

	// Create characteristics, set initial values, and set storage in NVS to true

	Characteristic::Active active{0, true};
	Characteristic::CurrentRelativeHumidity humidity{70, true};
	Characteristic::CurrentHumidifierDehumidifierState currentState{0, true};
	Characteristic::TargetHumidifierDehumidifierState targetState{0, true};
	Characteristic::RelativeHumidityHumidifierThreshold humidThreshold{40, true};
	Characteristic::RelativeHumidityDehumidifierThreshold dehumidThreshold{80, true};

	HumidifierDehumidifier() : Service::HumidifierDehumidifier()
	{
		targetState.setValidValues(1, 2); // on allow Dehumidifier State
	}

	boolean update() override
	{

		if (active.updated())
		{
			alorair.setPower(active.getNewVal());
		}

		if (dehumidThreshold.updated() && dehumidThreshold.getNewVal<float>() != dehumidThreshold.getVal<float>())
			alorair.setTargetHumidity(dehumidThreshold.getNewVal());

		return (true);
	}

	void loop() override
	{
		//  push state to HomeKit every 5 seconds, using the values that
		//  pollDehumidifier() reads from the CAN bus
		if (currentState.timeVal() > 5000)
		{
			if (!alorair.isConnected())
			{
				if (currentState.getVal() != 0)
					currentState.setVal(0); // CAN unavailable — show inactive
				return;
			}

			if (active.getVal() == 0)
			{
				if (currentState.getVal() != 0)
				{
					Serial.printf("Humidifier/DeHumidifier State: INACTIVE\n");
					currentState.setVal(0);
				}
				return;
			}

			currentState.setVal(alorair.currentStatus ? 3 : 1);
		}

		if (!alorair.isConnected())
			return;

		if (humidity.timeVal() > 5000 && alorair.currentHumidity >= 0 && alorair.currentHumidity <= 100)
		{
			if (fabs(humidity.getVal<float>() - alorair.currentHumidity) > 0.25)
				humidity.setVal(alorair.currentHumidity);
		}
	}
};

// temperature sensor structure
struct TempSensor : Service::TemperatureSensor
{
	SpanCharacteristic *temp;

	TempSensor() : Service::TemperatureSensor()
	{
		temp = new Characteristic::CurrentTemperature(0);
		temp->setRange(0, 110);
	}

	void loop() override
	{
		if (!alorair.isConnected() || temp->timeVal() < 5000)
			return;
		if (alorair.currentTemperature >= 0 && alorair.currentTemperature <= 110)
			temp->setVal(alorair.currentTemperature);
	}
};

// humidity sensor structure
struct HumiditySensor : Service::HumiditySensor
{
	SpanCharacteristic *humidity;

	HumiditySensor() : Service::HumiditySensor()
	{
		humidity = new Characteristic::CurrentRelativeHumidity(0);
		humidity->setRange(0, 100);
	}

	void loop() override
	{
		if (!alorair.isConnected() || humidity->timeVal() < 5000)
			return;
		if (alorair.currentHumidity >= 0 && alorair.currentHumidity <= 100)
			humidity->setVal(alorair.currentHumidity);
	}
};

////////////////////////////////////////////////////////////////////////

void setup()
{

	Serial.begin(115200);

#ifdef OTA_PASSWORD
	homeSpan.enableOTA(OTA_PASSWORD); // from secrets.h; survives factory reset
#else
	homeSpan.enableOTA(); // password stored in NVS (serial 'O' command)
#endif
	webPortalInit(); // captive-portal Wi-Fi setup + dashboard on DASHBOARD_PORT
	// Hostname "dehumidifier-<mac>.local", from the chip's factory MAC (eFuse),
	// so it never changes, even after a factory reset gives HomeKit a new ID.
	uint8_t mac[6];
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	static char hostSuffix[16];
	snprintf(hostSuffix, sizeof(hostSuffix), "-%02x%02x%02x%02x%02x%02x",
			 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	homeSpan.setHostNameSuffix(hostSuffix);
	homeSpan.begin(Category::Humidifiers, "HomeSpan Dehumidifier", HOSTNAME_BASE);


	new SpanAccessory();
	new Service::AccessoryInformation();
		new Characteristic::Identify();
		new Characteristic::Name("AlorAirHDi65");
		new Characteristic::FirmwareRevision(FW_VERSION);
		new Characteristic::Manufacturer("AlorAir");


	new HumidifierDehumidifier();
	new TempSensor();
	new HumiditySensor();

	alorair.currentTemperature = 0;
	alorair.currentHumidity = 0;

	if (alorair.begin())
	 	{
			LOG1("Can connected...");
		}
		else
		{
			LOG1("Can connect failed...");
		}

}

////////////////////////////////////////////////////////////////////////

// Read the dehumidifier's status over CAN on a fixed schedule. HomeKit and
// the dashboard both use the cached values. A failed read blocks for up to
// 400ms, so retry less often while the link is down.
static void pollDehumidifier()
{
	static uint32_t lastPoll = 0;
	uint32_t interval = alorair.isConnected() ? CAN_POLL_MS : CAN_RETRY_MS;
	if (millis() - lastPoll < interval)
		return;
	lastPoll = millis();
	alorair.status();
}

void loop()
{
	pollDehumidifier();
	homeSpan.poll();
	webPortalLoop();
}

////////////////////////////////////////////////////////////////////////