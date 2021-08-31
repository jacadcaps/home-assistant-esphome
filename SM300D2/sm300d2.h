#include "esphome.h"

class SM300D2 : public PollingComponent, public UARTDevice {

        class measurementResult {
		uint8_t _frameStart[2];
 		uint8_t _co2[2];
 		uint8_t _ch20[2];
 		uint8_t _tvoc[2];
 		uint8_t _pm25[2];
 		uint8_t _pm10[2];
 		uint8_t _temperature[2];
 		uint8_t _humidity[2];
 		uint8_t _chksum;
	public:

		float co2() const {
			return _co2[0] * 256 + _co2[1];
		}

		float ch20() const {
			return _ch20[0] * 256 + _ch20[1];
		}

		float tvoc() const {
			return _tvoc[0] * 256 + _tvoc[1];
		}

		float pm25() const {
			return _pm25[0] * 256 + _pm25[1];
		}

		float pm10() const {
			return _pm10[0] * 256 + _pm10[1];
		}

		float temperature() const {
			float out = _temperature[0] + (float(_temperature[1]) * 0.1f);
			return out;
		}

		float humidity() const {
			float out = _humidity[0] + (float(_humidity[1]) * 0.1f);
			return out;
		}

		bool isCRCValid() const {
			uint8_t sum = 0;
			const uint8_t *in = (const uint8_t *)this;
 			for (size_t i = 0; i < 16; i++) {
 				sum += in[i];
 			}
			return sum == _chksum;
		}

		bool isSane() const {
			uint32_t v;

			v = co2();
			if (v < 350 || v > 5550)
				return false;

			v = ch20();
			if (v > 1000)
				return false;

			v = tvoc();
			if (v > 2000)
				return false;

			v = pm25();
			if (v > 1000)
				return false;

			v = pm10();
			if (v > 1000)
				return false;

			float f = temperature();
			if (f < -40.f || f > 125.f)
				return false;

			f = humidity();
			if (f > 100.f)
				return false;

			return true;
		}
        };
        static_assert(sizeof(measurementResult) == 17, "incorrect data struct mapping");

	template<int T, int F = 30> class movingAverageSensor : public Sensor {
		std::function<float(void)> _getter;
		float                      _data[T];
		float                      _value;
		int                        _entries;
		uint32_t                   _count;

	static_assert(T >= 10, "use at least 10 entries");
	public:
		movingAverageSensor(std::function<float(void)>&&getter)
			: _getter(std::move(getter))
			, _value(0)
			, _entries(0)
			, _count(0)
		{
		}

		void update() {
			float value = _getter();

			if (_entries < T) {
				_data[_entries++] = value;
				_value += value;
			}
			else {
				_value -= _data[0];
				memmove(&_data[0], &_data[1], sizeof(float) * (T - 1));
				_data[T-1] = value;
				_value += value;
			}

			if (++_count == F) {
				publish_state(this->value());
				_count = 0;
			}
		}

		float value() {
			return _value / float(_entries);
		}
	};

 public:
	typedef movingAverageSensor<360, 30> smSensor;
	static const int _numSensors = 7;
	smSensor *         _s[_numSensors];
	measurementResult  _r;
	int                _errors = 0;
        SM300D2(UARTComponent *parent) : PollingComponent(2000), UARTDevice(parent) {
		_s[0] = new smSensor([this]{ return _r.pm25(); });
		_s[1] = new smSensor([this]{ return _r.pm10(); });
		_s[2] = new smSensor([this]{ return _r.co2(); });
		_s[3] = new smSensor([this]{ return _r.ch20(); });
		_s[4] = new smSensor([this]{ return _r.tvoc(); });
		_s[5] = new smSensor([this]{ return _r.temperature(); });
		_s[6] = new smSensor([this]{ return _r.humidity(); });
	}

	Sensor *s(int index) {
		return _s[index];
	}

        void setup() override {
        }

	void update() override {
		uint8_t *in = (uint8_t *)&_r;

		// We often end up with garbage data in the buffer
		// Let's just skip until we find a documented frame start

		// Find the 1st byte..
		in[0] = 0;
		for (int i = 0; i < 100; i++)
		{
			if (read_byte(&in[0]) && in[0] == 0x3c)
				break;
		}

		// Find the 2nd byte...
		if (!read_byte(&in[1]) || in[1] != 0x2)
		{
			_errors ++;
                        ESP_LOGD("custom", "Failed locating start frame, %d errors", _errors);
			return;
		}

		// Read the rest of the frame - this writes values into the measurementResult object
                if (read_array(&in[2], sizeof(_r) - 2))
                {
			// Validate the CRC
			if (!_r.isCRCValid()) {
				_errors ++;
        	                ESP_LOGD("custom", "Skipping readout due to CRC mismatch, %d errors", _errors);
				return;
			}

			// Ignore if any of the readout values are bogus - appears to be happening
			// quite often with some sensors at least...
			if (!_r.isSane()) {
				_errors ++;
        	                ESP_LOGD("custom", "Skipping readout due to out of range values, %d errors", _errors);
				return;
			}

			// Update the sensors...
			for (int i = 0; i < _numSensors; i++)
				_s[i]->update();

			_errors = 0;
		}
                else
                {
			_errors++;
                        ESP_LOGD("custom", "Failed reading data packet, errors %d", _errors);
			flush();
                }
	}
};

