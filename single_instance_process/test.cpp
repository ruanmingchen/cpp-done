#include "single_instance_process.h"

int main()
{
    SingleInstanceProcess single;
    single.CheckOnly();
    signal(SIGINT, SingleInstanceProcess::SigHandler);
    signal(SIGTERM, SingleInstanceProcess::SigHandler);
    SingleInstanceProcess::Run();
    return 0;
}