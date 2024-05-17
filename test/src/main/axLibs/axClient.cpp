#include "axClient.h"

int AxClient::chunkedConn(const char *type)
{
    // connect to server
    if (!connect())
    {
        return returnError(HTTPC_ERROR_CONNECTION_REFUSED);
    }

    addHeader("Connection", "Keep-Alive");
    addHeader("Transfer-Encoding", "chunked");
    // send Header
    if (!sendHeader(type))
    {
        return returnError(HTTPC_ERROR_SEND_HEADER_FAILED);
    }

    return 0;
}

int AxClient::chunkedSend(char *buff, size_t buffLen, int retry)
{
    // chunkedSize计算
    size_t buffLenZ = 0;
    size_t buffLenX = buffLen;
    while (buffLenX > 0)
    {
        buffLenZ++;
        buffLenX = buffLenX / 16;
    }

    // 数据格式
    char b = buff[0];
    char e = buff[buffLen - 1];
    // memcpy buffLen + buffLenZ + 2
    memcpy(buff + buffLenZ + 2, buff, buffLen + buffLenZ + 2);
    sprintf(buff, "%zx", buffLen);
    // buff[buffLenZ] = 0;
    // Serial.println(" chunkedSend " + String(buff) + ", " + String(buffLen));
    buff[buffLenZ] = '\r';
    buff[buffLenZ + 1] = '\n';
    buffLen = buffLen + buffLenZ + 4;
    buff[buffLen - 2] = '\r';
    buff[buffLen - 1] = '\n';

    // Serial.println(String((int)b) + " = " + String((int)buff[buffLenZ + 2]));
    // Serial.println(String((int)e) + " = " + String((int)buff[buffLen - 3]));

    // 数据写入
    return chunkedWrite(buff, buffLen, retry);
}

int AxClient::chunkedWrite(const char *buff, size_t buffLen, int retry)
{
    size_t bytesLeft = buffLen;
    for (int i = 0; i < retry; i++)
    {
        if (!connect())
        {
            return returnError(HTTPC_ERROR_CONNECTION_REFUSED);
        }

        // check for write error
        if (i > 0 && _client->getWriteError())
        {
            log_d("stream write error %d", _client->getWriteError());

            // reset write error for retry
            _client->clearWriteError();
        }

        int bytesWrite = _client->write((const uint8_t *)buff, bytesLeft);
        if (bytesWrite >= bytesLeft)
        {
            bytesLeft = 0;
            break;
        }

        log_d("short write, asked for %d but got %d retry...", bytesLeft, bytesWrite);

        buff += bytesWrite;
        bytesLeft -= bytesWrite;
        // some time for the stream
        delay(1);
    }

    if (bytesLeft > 0)
    {
        return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
    }

    // check for write error
    if (_client->getWriteError())
    {
        log_d("stream write error %d", _client->getWriteError());
        return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
    }

    delay(0);
    return 0;
}

int AxClient::chunkedRespone()
{
    chunkedWrite("0\r\n\r\n", 5, 2);
    return returnError(handleHeaderResponse());
}