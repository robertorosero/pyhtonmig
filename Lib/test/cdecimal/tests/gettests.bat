@ECHO OFF
if not exist testdata mkdir testdata
if not exist testdata\add.decTest copy /y ..\..\decimaltestdata\* testdata && move testdata\randomBound32.decTest testdata\randombound32.decTest && move testdata\remainderNear.decTest testdata\remaindernear.decTest
if not exist testdata\baseconv.decTest copy /y testdata_dist\* testdata
