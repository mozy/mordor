#include "mordor/statistics.h"
#include "mordor/test/test.h"

using namespace Mordor;

MORDOR_UNITTEST(Statistics, dumpStat)
{
    SumStatistic<int> sumStat("s");
    sumStat.add(5);
    std::ostringstream os;
    sumStat.dump(os);
    std::ostringstream expectedOS;
    expectedOS << typeid(sumStat).name() << ": "
        << sumStat << " s" << std::endl;
    MORDOR_TEST_ASSERT_EQUAL(os.str(), expectedOS.str());
}
