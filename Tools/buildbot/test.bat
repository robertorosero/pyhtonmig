@rem Used by the buildbot "test" step.
cd PCbuild
call rt.bat -d -uall -v test_ttk_guionly
call rt.bat -d -q -uall -rw -x test_ttk_guionly
