#ifndef AX_CLIENT_H
#define AX_CLIENT_H

#include <HTTPClient.h>

class AxClient : public HTTPClient
{
public:
    int chunkedConn(const char *type);
    int chunkedSend(char *buff, size_t buffLen, int retry = 2);
    int chunkedRespone();
};

#endif