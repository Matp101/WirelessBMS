#ifndef FUELGAUGE_H_
#define FUELGAUGE_H_
#include <Arduino.h>

//==============================================================================
// Editable settings

#define TABLE_SIZE 32


//==============================================================================


#if defined(DEBUG_SERIAL) && !defined(PRINTF) 
#define PRINTF(...) { Serial.printf(__VA_ARGS__); }
#endif


class FuelGauge
{
private:
    float _Q;    // Battery capacity in Ah
    float _R;    // Battery resistance in Ohms
    float _Vmin; // Minimum discharge voltage
    float _IL;   // Load current in A

    unsigned long _t_prev;      // previous time
    float _SOC_prev;            // initial values

    float SOC;      // State of charge
    float TTS;      // Time to empty
    float _SOCt;    // SOC at time t

    int _cell_count = 4;
    bool _updated_table = false;

    // SOC table data
    float _SOC_TABLE[TABLE_SIZE] = {0, 0.0322580645161290, 0.0645161290322581, 0.0967741935483871, 0.129032258064516, 0.161290322580645, 0.193548387096774, 0.225806451612903, 0.258064516129032, 0.290322580645161, 0.322580645161290, 0.354838709677419, 0.387096774193548, 0.419354838709677, 0.451612903225806, 0.483870967741936, 0.516129032258065, 0.548387096774194, 0.580645161290323, 0.612903225806452, 0.645161290322581, 0.677419354838710, 0.709677419354839, 0.741935483870968, 0.774193548387097, 0.806451612903226, 0.838709677419355, 0.870967741935484, 0.903225806451613, 0.935483870967742, 0.967741935483871, 1};

    // OCV table data (assumes number of series cells Ns = 4)
    float _OCV_TABLE[TABLE_SIZE] = {2.50606500000000, 3.06704350805968, 3.22721595931014, 3.32644969320877, 3.38904116031604, 3.43616424799932, 3.46849522808742, 3.50101121522852, 3.53858030642213, 3.57272973938998, 3.59980948232041, 3.62433635625129, 3.64682644413282, 3.67036279128122, 3.69667500069104, 3.72629988260380, 3.75724354052041, 3.78787042065915, 3.82198062159718, 3.85712587710054, 3.88660822107927, 3.91029552887068, 3.93400984868228, 3.96547919092361, 4.00415596526421, 4.04067873431694, 4.06598519698050, 4.07411388711950, 4.08055948481741, 4.09199332342299, 4.11917903089285, 4.19316500000000};


    float _SOCLookup(double ocv)
    {
        int i;
        // Find closest x values in the table (not the best approach)
        for (i = 0; i < TABLE_SIZE - 1; i++)
        {
            if (ocv >= _OCV_TABLE[i] && ocv <= _OCV_TABLE[i + 1])
            {
                break;
            }
        }
        // Perform linear interpolation
        float x0 = _OCV_TABLE[i];
        float x1 = _OCV_TABLE[i + 1];
        float y0 = _SOC_TABLE[i];
        float y1 = _SOC_TABLE[i + 1];
        float soc = y0 + (y1 - y0) * (ocv - x0) / (x1 - x0);
        return soc;
    }

    void _updateCellCount(int cell_count){
        if(cell_count == _cell_count){
            return;
        }
        if (_updated_table == true){
            for (int i = 0; i < TABLE_SIZE; i++)
            {
                float ocv = _OCV_TABLE[i];
                _OCV_TABLE[i] = ocv / float(_cell_count);
            }
            _R = _R / float(_cell_count);
            _Vmin = _Vmin / float(_cell_count);
        }
        for(int i = 0;i < TABLE_SIZE; i++){
            float ocv = _OCV_TABLE[i];
            _OCV_TABLE[i] = float(cell_count) * ocv;
        }
        _R = _R * float(cell_count);
        _Vmin = _Vmin * float(cell_count);
        _updated_table = true;

        _cell_count = cell_count;
    }

public:
    FuelGauge(float capacity = 4.2f, float internal_resistance = 0.001f, float Vmin = 2.7f, float load = 0.250f, int cell_count = 4)
    {
        this->_Q = capacity;
        this->_R = internal_resistance;
        this->_Vmin = Vmin;
        this->_IL = load;

        // Initialize previous values
        this->_t_prev = 0;
        this->_SOC_prev = 0.0f;

        // Update cell count
        this->_updateCellCount(cell_count);

        // Initial TTS
        float OCVt = _Vmin - _IL * _R;
        float _SOCt = _SOCLookup(OCVt);
    }

    void initialize(float I0, float V0, float *SOC0, float *TTS0)
    {
        // Initial SOC
        float OCV0 = V0 - I0 * _R;
        *SOC0 = _SOCLookup(OCV0);

        float SOCd = *SOC0 - _SOCt;
        *TTS0 = SOCd * _Q / _IL;

        // Update previous values
        _t_prev = millis() / 1000; // seconds
        _SOC_prev = *SOC0;
    }

    void loop(float I_k, float *SOC_k, float *TTS_k)
    {
        unsigned long delta_k = (millis() / 1000) - _t_prev; // seconds
        float SOC_km1 = _SOC_prev;                  // SOC at k-1

        // SOC at k
        *SOC_k = SOC_km1 + I_k * float(delta_k) / _Q;

        // TTS at [k]
        float SOCd = *SOC_k - _SOCt;
        *TTS_k = SOCd * _Q / _IL;

        // Update SOC
        SOC = *SOC_k;
        TTS = *TTS_k;
    }

    float getSOC()
    {
        return SOC;
    }

    float getTTS()
    {
        return TTS;
    }
};

#endif // FUELGAUGE_H_