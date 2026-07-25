// test_new_host.c
// Unlicense

#include<liblimeade.h>

#define CHECK()                    \
{                                  \
    int code = limeade_poperror(); \
    if (code != LIMEADE_SUCCESS)   \
    {                              \
        return code;               \
    }                              \
}

int main(int argc, char **argv)
{
    LIMEADE_CONTEXT self;
    LIMEADE_PACKET_EVENT evnt;

    self = limeade_host_init();
}
