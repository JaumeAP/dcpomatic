set DCPOMATIC_TEST_PRIVATE=c:\users\ci\dcpomatic-test-private
xcopy ..\libdcp\tags build\test\tags\
copy ..\libdcp\ratings build\test\
copy ..\openssl\apps\openssl.exe build\test\
copy fonts\fonts.conf.windows fonts\fonts.conf

if "%1" == "2057" (
  REM en-DE
  set PATH=%PATH%;c:\users\ci\bin_v2.18.x;c:\users\ci\workspace\dcpomatic\bin;c:\users\ci\workspace\dcpomatic\lib
  build\test\lib\unit-tests.exe --log_level=test_suite %2 %3 -t !i18n_test_de_de
  build\test\wx\unit-tests.exe --log_level=test_suite %2 %3 -t !i18n_test_de_de
) else (
  REM Presumably de-DE
  set PATH=%PATH%;c:\users\ci-de\bin_v2.18.x;c:\users\ci-de\workspace\dcpomatic\bin;c:\users\ci-de\workspace\dcpomatic\lib
  build\test\lib\unit-tests.exe --log_level=test_suite -t i18n_test_de_de
  build\test\wx\unit-tests.exe --log_level=test_suite -t i18n_test_de_de
)


