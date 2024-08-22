#include "axWifi.h"

#define PRE_KEY_WIFI_SSID "wifi.ssid"
#define PRE_KEY_WIFI_PASSWD "wifi.passwd"

bool axWifiConnected = false;
int axWifiConnSeq = 0;

String _ssid;
String _passwd;
wl_status_t _status = WL_NO_SHIELD;
int _event = -1;
bool _conning = false;
unsigned long _statusCheckNext = 0;
unsigned long _statusReconnNext = 0;

void axWifiEvent(WiFiEvent_t event)
{
    if (_event == event)
    {
        return;
    }

    _event = event;
    _statusCheckNext = 0;
    Serial.println("axWifiEvent : " + String(event));
}

void axWifiConnDo(const char *ssid, const char *passwd)
{
    _event = -1;
    _statusReconnNext = millis() + AX_WIFI_RECONN_INTERVAL;
    Serial.println("axWifi connect " + _ssid);
    WiFi.disconnect();
    WiFi.begin(ssid, passwd);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
}

void axWifiConn(const char *ssid, const char *passwd)
{
    _ssid = ssid;
    _passwd = passwd;
    _conning = true;
    axPreferences.putString(PRE_KEY_WIFI_SSID, _ssid);
    axPreferences.putString(PRE_KEY_WIFI_PASSWD, _passwd);
    axWifiConnDo(ssid, passwd);
}

void axWifiInit()
{
    WiFi.onEvent(axWifiEvent);
    _ssid = axPreferences.getString(PRE_KEY_WIFI_SSID);
    _passwd = axPreferences.getString(PRE_KEY_WIFI_PASSWD);
}

void axWifiLoop()
{
    if (_ssid.length() <= 0)
    {
        return;
    }

    unsigned long currentMillis = millis();
    if (_statusCheckNext < currentMillis)
    {
        _statusCheckNext = currentMillis + AX_WIFI_CHECK_INTERVAL;
        wl_status_t status = WiFi.status();
        _status = status;
        if (_conning && (status == WL_CONNECTED || status == WL_DISCONNECTED))
        {
            _conning = false;
            axWifiConnSeq = axWifiConnSeq >= 65535 ? 0 : (axWifiConnSeq + 1);
        }

        if (status == WL_CONNECTED)
        {
            if (!axWifiConnected || _conning)
            {
                axWifiConnected = true;
                Serial.println("axWifi connected " + _ssid);
            }
        }
        else
        {
            if (axWifiConnected)
            {
                axWifiConnected = false;
                Serial.println("axWifi disConnected " + _ssid);
            }

            if (_statusReconnNext < currentMillis)
            {
                axWifiConnDo(_ssid.c_str(), _passwd.c_str());
            }
        }
    }
}

bool axWifiConnKeep(long wait)
{
    if (axWifiConnected)
    {
        return true;
    }

    axWifiLoop();
    if (wait == 0)
    {
        wait = AX_WIFI_CONN_KEEP_WAIT;
    }

    wait += millis();
    while (true)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            axWifiConnected = true;
            return true;
        }

        if (wait < millis())
        {
            return false;
        }

        delay(100);
    }
}