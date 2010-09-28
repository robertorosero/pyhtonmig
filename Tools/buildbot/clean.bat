@rem Used by the buildbot "clean" step.
call "%VS90COMNTOOLS%vsvars32.bat"
@echo Deleting .pyc/.pyo files ...
del /s Lib\*.pyc Lib\*.pyo
cd PCbuild
vcbuild /useenv kill_python.vcproj "Release|Win32" && kill_python.exe
vcbuild /useenv kill_python.vcproj "Debug|Win32" && kill_python_d.exe
vcbuild /clean pcbuild.sln "Release|Win32"
vcbuild /clean pcbuild.sln "Debug|Win32"
