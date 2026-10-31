#include "utils/console.h"
#include "utils/logger.h"
#include <auth/auth.h>

int entry_point();

int main()
{
    console->initialize();
    logger_core::log_info("welcome to fragment!");
    logger_core::log_info("build : {} {}", __DATE__, __TIME__);

    if (!LoadOffsets())
    {
        logger_core::log_error("couldn't load offsets");
        return 1;
    }

    return entry_point();
}
