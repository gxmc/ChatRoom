#include "../src/Server.h"

#if __cplusplus < 201103
    #error "use c++11 at least"
#endif

int main()
{
    chat::Server server;
    server.init();
    server.eventLoop();
}
