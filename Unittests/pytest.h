#ifndef UNITTESTS_PYTEST_H_
#define UNITTESTS_PYTEST_H_

#include "Python.h"

typedef enum {
    ImportSite = 0,  // Import site.py (the default).
    NoSite           // Do not import site.py (python -S).
} NoSiteFlag;

/// RAII-style class for common Python setup/teardown test fixture routines.
/// By default, PythonSetupTeardown will not import site.py (like Python's -S
/// option). If you need site.py, use PythonSetupTeardown(ImportSite).
class PythonSetupTeardown {
public:
    PythonSetupTeardown(NoSiteFlag no_site_flag = NoSite)
    {
        this->orig_no_site_flag_ = Py_NoSiteFlag;
        Py_NoSiteFlag = no_site_flag;
        Py_Initialize();
    }

    ~PythonSetupTeardown()
    {
        if (PyErr_Occurred()) {
            PyErr_Print();
            PyErr_Clear();
        }
        Py_Finalize();
        Py_NoSiteFlag = this->orig_no_site_flag_;
    }

private:
    // Original value of Py_NoSiteFlag.
    bool orig_no_site_flag_;
};

#endif