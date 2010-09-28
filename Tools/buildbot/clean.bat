@rem Used by the buildbot "clean" step.
call "%VS90COMNTOOLS%vsvars32.bat"
vcbuild /useenv PCbuild\kill_python.vcproj "Debug|Win32" && PCbuild\kill_python_d.exe
@echo Deleting .pyc/.pyo files ...
del /s Lib\*.pyc Lib\*.pyo
cd PCbuild
vcbuild /clean pcbuild.sln "Release|Win32"
vcbuild /clean pcbuild.sln "Debug|Win32"
