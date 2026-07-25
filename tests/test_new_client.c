// test_new.c
// Unlicense

#include<liblimeade.h>

#define PORT 22
#define HOST "127.0.0.1"
#define HOST_USER "limeade"

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

    self = limeade_client_init(PORT, HOST, HOST_USER);
    CHECK();

    {
        LIMEADE_PACKET_SYSOVERV overv;
        overv.hostname  = "fedora-test";
        overv.kernelver = "7.0.12-fedora1-1";
        overv.distro    = "Fedora Linux";
        overv.processor = "Core i5-5700KU";
        overv.processor_vend = "inGenuineIntel";
        overv.ram_gbs   = 13;
        overv.prev_sessionid = 0xBEEFEEC0DE; // char[5] equivalent

        limeade_send(self, overv);
        CHECK();

        while (true)
        {
            LIMEADE_RECVED recvd = limeade_wait_recv(self);
            CHECK();

            if (recvd.type == LIMEADE_HOST_ANSWER)
            {
                LIMEADE_PACKET_ANSWER resp = limeade_process_answer(self, recvd);
                CHECK();

                self.sessionid = resp.sessionid;
            
                break;
            }
        }
    }

    while(true)
    {
        evnt.ts     = {1000000000, 500};
        evnt.pid    = 69420;
        evnt.type   = "getpid";
        evnt.arg1   = "";
        evnt.arg2   = "";
        evnt.retval = 69420;
    
        limeade_send(self, evnt);
        CHECK();
    
        evnt.ts     = {1000000000, 501};
        evnt.pid    = 42069;
        evnt.type   = "kill";
        evnt.arg1   = "69420";
        evnt.arg2   = "11"; // SIGSEGV
        evnt.retval = 0;

        limeade_send(self, evnt);
        CHECK();

        evnt.ts     = {1000000000, 510};
        evnt.pid    = 42069;
        evnt.type   = "fork";
        evnt.arg1   = "";
        evnt.arg2   = "";
        evnt.retval = 69420;

        limeade_send(self, evnt);
        CHECK();

        evnt.ts     = {1000000000, 515};
        evnt.pid    = 69420;
        evnt.type   = "execve";
        evnt.arg1   = "/proc/self/exe";
        evnt.arg2   = "";
        evnt.retval = 0; // I guess?
    
        limeade_send(self, evnt);
        CHECK();
    
    }

    return 0;
}

