#include "Python.h"
#include "datetime.h"

#include "gtest/gtest.h"
#include "pytest.h"

// Grab common setup/teardown routines.
class DatetimeTest : public ::testing::Test {
public:
    DatetimeTest(): setup_teardown_(ImportSite) {}

private:
    PythonSetupTeardown setup_teardown_;
};


TEST_F(DatetimeTest, ImportCApi)
{
    PyDateTime_IMPORT;

    EXPECT_TRUE(PyDateTimeAPI != NULL);
}