#include "axClient.h"

int AxClient::chunkedConn(const char *type)
{
    // connect to server
    if (!connect())
    {
        return returnError(HTTPC_ERROR_CONNECTION_REFUSED);
    }

    // send Header
    if (!sendHeader(type))
    {
        return returnError(HTTPC_ERROR_SEND_HEADER_FAILED);
    }

    return 0;
}

int AxClient::chunkedSend(char *buff, size_t buffLen, int retry)
{
    if (!connect())
    {
        return returnError(HTTPC_ERROR_CONNECTION_REFUSED);
    }

    size_t bytesLeft = buffLen;
    for (int i = 0; i < retry; i++)
    {
        int bytesWrite = _client->write((const uint8_t *)buff, bytesLeft);
        if (bytesWrite >= bytesLeft)
        {
            bytesLeft = 0;
            break;
        }

        log_d("short write, asked for %d but got %d retry...", bytesLeft, bytesWrite);
        // check for write error
        if (_client->getWriteError())
        {
            log_d("stream write error %d", _client->getWriteError());

            // reset write error for retry
            _client->clearWriteError();
        }

        buff += bytesWrite;
        bytesLeft -= bytesWrite;
        // some time for the stream
        delay(1);
    }

    if (bytesLeft > 0)
    {
        return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
    }

    delay(0);
    return 0;
}

int AxClient::chunkedRespone()
{
    return returnError(handleHeaderResponse());
}