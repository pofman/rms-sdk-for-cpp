
cd ../sdk
export XKB_DEFAULT_RULES=base
qmake
make clean
make
if [ $? -ne 0 ]; then
  exit 1
fi
cd ../samples
qmake
make clean
make
if [ $? -ne 0 ]; then
  exit 1
fi
cd ../bin
export LD_LIBRARY_PATH=`pwd`
cd tests
./rmscryptoUnitTests -xunitxml > rmsUnitTestResults.txt
./rmsauthUnitTests -xunitxml >> rmsUnitTestResults.txt
./rmsplatformUnitTests -xunitxml >> rmsUnitTestResults.txt
./RestClientsUnitTests -xunitxml >> rmsUnitTestResults.txt
./xmpFileUnitTests -xunitxml >> rmsUnitTestResults.txt
./commonUnitTests -xunitxml >> rmsUnitTestResults.txt
cd ../../tools
powershell ./Rms_Sdk_For_Cpp_UTResults_Parser.ps1
