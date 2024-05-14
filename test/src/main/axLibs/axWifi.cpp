#include "axWifi.h"

#define PRE_KEY_WIFI_SSID "wifi.ssid"
#define PRE_KEY_WIFI_PASSWD "wifi.passwd"

static String _ssid;
static String _passwd;
static wl_status_t _status = WL_NO_SHIELD;
static unsigned long _statusCheckNext = 0;
static unsigned long _statusReconnNext = 0;

void axWifiConnDo(const char *ssid, const char *passwd)
{
    _statusReconnNext = millis() + AX_WIFI_RECONN_INTERVAL;
    WiFi.disconnect();
    WiFi.begin(ssid, passwd);
}

void axWifiConn(const char *ssid, const char *passwd)
{
    _ssid = ssid;
    _passwd = passwd;
    axPreferences.putString(PRE_KEY_WIFI_SSID, _ssid);
    axPreferences.putString(PRE_KEY_WIFI_PASSWD, _passwd);
    axWifiConnDo(ssid, passwd);
}

void axWifiInit()
{
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
        if (_status == status)
        {
            return;
        }

        _status = status;
        if (status == WL_CONNECTED)
        {
            if (!axWifiConnected)
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