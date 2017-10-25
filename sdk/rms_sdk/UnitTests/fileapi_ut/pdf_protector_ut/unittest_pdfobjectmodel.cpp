#include "Auth.h"
#include "depend.h"
#include "PDFObjectModel/PDFObjectModel.h"
#include "UserPolicy.h"
#include "PDFProtector_child.h"
#include "PFileProtector.h"
#include "Core/ProtectionPolicy.h"
using namespace std;
using namespace rmsauth;
using namespace rmscore::pdfobjectmodel;
using namespace rmscore::fileapi;
using namespace rmscore::core;
using namespace rmscore::modernapi;


class PDFCreator_CreateCustomEncryptedFile;
class PDFCreator_UnprotectCustomEncryptedFile;
class PDFWrapperDoc_GetWrapperType;
class PDFWrapperDoc_GetCryptographicFilter;
class PDFWrapperDoc_GetPayLoadSize;
class PDFWrapperDoc_GetPayloadFileName;
std::shared_ptr<UserPolicy> m_userPolicy;

void SetUserPolicy()
{
    PDFModuleMgr::Initialize();//init
    //**********get template info**************
    AuthCallback auth(CLIENTID, REDIRECTURL);
    ConsentCallback consent;
    AppDataHashMap signedData;
    std::shared_ptr<std::atomic<bool> > cancelState(new std::atomic<bool>(false));
        auto templatesFuture = TemplateDescriptor::GetTemplateListAsync(
          CLIENTEMAIL, auth, launch::deferred, cancelState);
    auto templates = templatesFuture.get();
    ProtectWithTemplateOptions pt (CryptoOptions::AES128_ECB,(*templates)[0], signedData, true);//设置模板
    //**********************************
    UserContext ut (CLIENTEMAIL, auth, consent);

    auto userPolicyCreationOptions = UserPolicyCreationOptions::USER_AllowAuditedExtraction;
    userPolicyCreationOptions = static_cast<rmscore::modernapi::UserPolicyCreationOptions>(
                          userPolicyCreationOptions |
                          rmscore::modernapi::UserPolicyCreationOptions::USER_PreferDeprecatedAlgorithms);

    m_userPolicy = UserPolicy::CreateFromTemplateDescriptor(pt.templateDescriptor,
                                                                             ut.userId,
                                                                             ut.authenticationCallback,
                                                                             userPolicyCreationOptions,
                                                                             pt.signedAppData,
                                                                            cancelState);
}

struct CreateCustomEncryptedFile_P{
      CreateCustomEncryptedFile_P(std::string fileIn_,
                                  std::string fileout_,
                                  std::string filterName_,
                                  std::string ExceptionsMess_,
                                  uint32_t ret_)
       {
           fileIn = fileIn_;
           fileout =fileout_;
           filterName = filterName_;
           ret = ret_;
           ExceptionsMess=ExceptionsMess_;
       }
           std::string fileIn;
           std::string fileout;
           std::string filterName;
           std::string ExceptionsMess;
           uint32_t ret;
 };

class PDFCreator_CreateCustomEncryptedFile:public ::testing::TestWithParam<CreateCustomEncryptedFile_P>
{
public:
 static void SetUpTestCase() {
    SetUserPolicy();
 }
 static void TearDownTestCase() {

 }
};

TEST_P(PDFCreator_CreateCustomEncryptedFile,CreateCustomEncryptedFile_T)
{
      CreateCustomEncryptedFile_P TParam=GetParam();
      std::string fileIn= GetCurrentInputFile() +TParam.fileIn;
      //std::string fileIns=GetCurrentInputFile() "Input/Protector/anyoness.pdf";

      auto inFile = make_shared<fstream>(
        fileIn, ios_base::in | ios_base::out | ios_base::binary);

      std::unique_ptr<PDFCreator> m_pdfCreator = PDFCreator::Create();

      auto encryptedSS = std::make_shared<std::stringstream>();
      std::shared_ptr<std::iostream> encryptedIOS = encryptedSS;
      auto outputEncrypted = rmscrypto::api::CreateStreamFromStdStream(encryptedIOS);

      std::string originalFileExtension=".pFile";

      auto p_PDFprotector = std::make_shared<PDFProtector_unit>(fileIn,originalFileExtension,inFile);
      auto cryptoHander = std::make_shared<PDFCryptoHandler_child>(p_PDFprotector);

      p_PDFprotector->SetUserPolicy(m_userPolicy);

      std::string filterName = TParam.filterName;
      std::vector<unsigned char> publishingLicense = m_userPolicy->SerializedPolicy();

      uint32_t ret;
      try{
          ret = m_pdfCreator->CreateCustomEncryptedFile(fileIn,
                                                        filterName,
                                                        publishingLicense,
                                                        cryptoHander,
                                                        outputEncrypted);
      }
      catch (const rmsauth::Exception& e)
      {
          std::string message(e.what());
          EXPECT_EQ(TParam.ExceptionsMess,message);
          return;
      }
      catch (const rmscore::exceptions::RMSPDFFileException& e)
      {
          std::string message(e.what());
          EXPECT_EQ(TParam.ExceptionsMess,message);
          return;
      }
      catch (const rmscore::exceptions::RMSException& e)
      {
          std::string message(e.what());
          EXPECT_EQ(TParam.ExceptionsMess,message);
          return;
      }
      EXPECT_EQ(TParam.ret,ret);
      if(ret==PDFCREATOR_ERR_SUCCESS)
      {
          //完全加密
          std::string wrapperIn= GetCurrentInputFile() +"Input/wrapper.pdf";


          auto inwrapper = make_shared<fstream>(
            wrapperIn, ios_base::in | ios_base::out | ios_base::binary);
          std::shared_ptr<std::iostream> inputWrapperIO = inwrapper;
          auto inputWrapper = rmscrypto::api::CreateStreamFromStdStream(inputWrapperIO);
          std::unique_ptr<PDFUnencryptedWrapperCreator> m_pdfWrapperCreator=PDFUnencryptedWrapperCreator::Create(inputWrapper);
          m_pdfWrapperCreator = PDFUnencryptedWrapperCreator::Create(inputWrapper);
          m_pdfWrapperCreator->SetPayloadInfo(
                      PDF_PROTECTOR_WRAPPER_SUBTYPE,
                      PDF_PROTECTOR_WRAPPER_FILENAME,
                      PDF_PROTECTOR_WRAPPER_DES,
                      PDF_PROTECTOR_WRAPPER_VERSION);
          m_pdfWrapperCreator->SetPayLoad(outputEncrypted);

          string fileOut = GetCurrentInputFile()+TParam.fileout;
          auto outputStream = make_shared<fstream>(
            fileOut, ios_base::in | ios_base::out | ios_base::trunc | ios_base::binary);
          std::shared_ptr<std::iostream> outputIO = outputStream;
          auto outputWrapper = rmscrypto::api::CreateStreamFromStdStream(outputIO);
          bool bResult = m_pdfWrapperCreator->CreateUnencryptedWrapper(outputWrapper);
          EXPECT_EQ(1,bResult);
      }
}
INSTANTIATE_TEST_CASE_P(,PDFCreator_CreateCustomEncryptedFile,testing::Values(
    CreateCustomEncryptedFile_P("Input/unprotector.pdf","OutPut/CreateCustomEncryptedFile/FoxitIRMServices.pdf","FoxitIRMServices","NO Exception",PDFCREATOR_ERR_SUCCESS),//不挂就好
    CreateCustomEncryptedFile_P("Input/package.pdf","OutPut/CreateCustomEncryptedFile/package.pdf",PDF_PROTECTOR_FILTER_NAME,"NO Exception",PDFCREATOR_ERR_SUCCESS),
    CreateCustomEncryptedFile_P("Input/demage.pdf","OutPut/CreateCustomEncryptedFile/demage.pdf",PDF_PROTECTOR_FILTER_NAME,"NO Exception",PDFCREATOR_ERR_FORMAT),
    CreateCustomEncryptedFile_P("Input/XFAStatic.pdf","OutPut/CreateCustomEncryptedFile/XFAStatic.pdf",PDF_PROTECTOR_FILTER_NAME,"NO Exception",PDFCREATOR_ERR_SUCCESS),
    CreateCustomEncryptedFile_P("Input/sign.pdf","OutPut/CreateCustomEncryptedFile/sign.pdf",PDF_PROTECTOR_FILTER_NAME,"NO Exception",PDFCREATOR_ERR_FORMAT),//未加密的文档
    CreateCustomEncryptedFile_P("Input/XFADyanmic-crash.pdf","OutPut/CreateCustomEncryptedFile/XFADyanmic-crash.pdf",PDF_PROTECTOR_FILTER_NAME,"NO Exception",PDFCREATOR_ERR_FORMAT),//未加密的文档
    CreateCustomEncryptedFile_P("Input/XFADyanmic.pdf","OutPut/CreateCustomEncryptedFile/XFADyanmic.pdf",PDF_PROTECTOR_FILTER_NAME,"NO Exception",PDFCREATOR_ERR_FORMAT),//未加密的文档
    //CreateCustomEncryptedFile_P("Input/Protector/cer2-no.pdf","OutPut/CreateCustomEncryptedFile/cer2-no.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SECURITY),
    CreateCustomEncryptedFile_P("Input/Protector/cer.pdf","OutPut/CreateCustomEncryptedFile/cer.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SECURITY),
    CreateCustomEncryptedFile_P("Input/error/anyone.pdf","OutPut/CreateCustomEncryptedFile/error.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FILE),
    CreateCustomEncryptedFile_P("Input/Protector/anyone.pdf","OutPut/CreateCustomEncryptedFile/anyone.pdf",PDF_PROTECTOR_FILTER_NAME,"NO Exception",PDFCREATOR_ERR_SUCCESS),
    CreateCustomEncryptedFile_P("Input/Protector/password.pdf","OutPut/CreateCustomEncryptedFile/password.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SECURITY),
    CreateCustomEncryptedFile_P("Input/Protector/pwd_protect.pdf","OutPut/CreateCustomEncryptedFile/pwd_protect.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SECURITY),
    CreateCustomEncryptedFile_P("Input/Test.txt","OutPut/CreateCustomEncryptedFile/txt.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),//未加密的文档
    CreateCustomEncryptedFile_P("Input/unprotector.pdf","OutPut/CreateCustomEncryptedFile/protector.pdf",PDF_PROTECTOR_FILTER_NAME,"NO Exception",PDFCREATOR_ERR_SUCCESS)//未加密的文档

      ));

struct UnprotectCustomEncryptedFile_P
{
    UnprotectCustomEncryptedFile_P(std::string fileIn_,std::string fileout_,std::string filterName_,std::string Exceptionmess_,uint32_t ret_)
    {
        fileIn = fileIn_;
        fileout =fileout_;
        filterName = filterName_;
        ExceptionsMess = Exceptionmess_;
        ret = ret_;

    }
        std::string fileIn;
        std::string fileout;
        std::string filterName;
        std::string ExceptionsMess;
        uint32_t ret;

};
class PDFCreator_UnprotectCustomEncryptedFile:public ::testing::TestWithParam<UnprotectCustomEncryptedFile_P>
{
public:
 static void SetUpTestCase() {


 }
 static void TearDownTestCase() {

 }
};
TEST_P(PDFCreator_UnprotectCustomEncryptedFile,UnprotectCustomEncryptedFile_T)
{
    UnprotectCustomEncryptedFile_P TParam = GetParam();
    std::string fileIn= GetCurrentInputFile() +TParam.fileIn;
    auto inFile = make_shared<fstream>(
      fileIn, ios_base::in | ios_base::out | ios_base::binary);
    std::string originalFileExtension=".pFile";

    auto p_PDFprotector = std::make_shared<PDFProtector_unit>(fileIn,originalFileExtension,inFile);

    //******************
    PDFModuleMgr::Initialize();

    //************************
    AuthCallback auth(CLIENTID, REDIRECTURL);
    ConsentCallback consent;
    rmscore::fileapi::UnprotectOptions upt (false, true);
    rmscore::fileapi::UserContext ut (CLIENTEMAIL, auth,consent);
    std::shared_ptr<std::atomic<bool> > cancelState(new std::atomic<bool>(false));
    //*****************************************
    auto securityHander = std::make_shared<PDFSecurityHandler_child>(p_PDFprotector,ut,upt,cancelState);
    //**************************************************************
    std::unique_ptr<PDFCreator> m_pdfCreator = PDFCreator::Create();
    //****************
    string fileOut = GetCurrentInputFile() +TParam.fileout;
    // create streams
    auto outputStream = make_shared<fstream>(
      fileOut, ios_base::in | ios_base::out | ios_base::trunc | ios_base::binary);
    std::shared_ptr<std::iostream> outputDecryptedIO = outputStream;
    auto outputDecrypted = rmscrypto::api::CreateStreamFromStdStream(outputDecryptedIO);
    std::string filterName = TParam.filterName;


    std::shared_ptr<std::iostream> inputEncryptedIO = inFile;
    auto inputEncrypted = rmscrypto::api::CreateStreamFromStdStream(inputEncryptedIO);
    std::unique_ptr<PDFWrapperDoc> pdfWrapperDoc =  PDFWrapperDoc::Create(inputEncrypted);
    auto payloadSS = std::make_shared<std::stringstream>();
    std::shared_ptr<std::iostream> payloadIOS = payloadSS;
    auto outputPayload = rmscrypto::api::CreateStreamFromStdStream(payloadIOS);
    bool bGetPayload = pdfWrapperDoc->StartGetPayload(outputPayload);
    //******************

    uint32_t ret ;
    try{
        ret= m_pdfCreator->UnprotectCustomEncryptedFile(
            outputPayload,
            filterName,
            securityHander,
            outputDecrypted);

    }
    catch (const rmsauth::Exception& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionsMess,message);
        return;
    }
    catch (const rmscore::exceptions::RMSPDFFileException& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionsMess,message);
        return;
    }
    catch (const rmscore::exceptions::RMSException& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionsMess,message);
        return;
    }
    EXPECT_EQ(TParam.ret,ret);
    //*************
}
INSTANTIATE_TEST_CASE_P(,PDFCreator_UnprotectCustomEncryptedFile,testing::Values(
 UnprotectCustomEncryptedFile_P("Input/Protector/Protected package.pdf","OutPut/UnprotectCustomEncryptedFile/Protected package.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SUCCESS),
 UnprotectCustomEncryptedFile_P("Input/Protector/customerTemplate.pdf","OutPut/UnprotectCustomEncryptedFile/customerTemplate.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SECURITY),
 UnprotectCustomEncryptedFile_P("Input/Protector/demage.pdf","OutPut/UnprotectCustomEncryptedFile/demage.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),
 UnprotectCustomEncryptedFile_P("Input/sign.pdf","OutPut/UnprotectCustomEncryptedFile/sign.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),
 UnprotectCustomEncryptedFile_P("Input/XFADyanmic-crash.pdf","OutPut/UnprotectCustomEncryptedFile/XFADyanmic-crash.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),
 UnprotectCustomEncryptedFile_P("Input/XFAStatic.pdf","OutPut/UnprotectCustomEncryptedFile/XFAStatic.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),
 UnprotectCustomEncryptedFile_P("Input/Protector/Protected XFAStatic.pdf","OutPut/UnprotectCustomEncryptedFile/Protected XFAStatic.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SUCCESS),
 //UnprotectCustomEncryptedFile_P("Input/Protector/Protected sign.pdf","OutPut/UnprotectCustomEncryptedFile/sign.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SUCCESS),
 //UnprotectCustomEncryptedFile_P("Input/Protector/Protected XFADyanmic-crash.pdf","OutPut/UnprotectCustomEncryptedFile/XFADyanmic-crash.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SUCCESS),
 //UnprotectCustomEncryptedFile_P("Input/Protector/Protected XFADyanmic.pdf","OutPut/UnprotectCustomEncryptedFile/XFADyanmic.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SUCCESS),
 //UnprotectCustomEncryptedFile_P("Input/Protector/cer2-no.pdf","OutPut/UnprotectCustomEncryptedFile/cer2-no.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),
 UnprotectCustomEncryptedFile_P("Input/Protector/cer.pdf","OutPut/UnprotectCustomEncryptedFile/cer.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),
 UnprotectCustomEncryptedFile_P("Input/Test.txt","OutPut/UnprotectCustomEncryptedFile/txt.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),
 UnprotectCustomEncryptedFile_P("Input/unprotector.pdf","OutPut/UnprotectCustomEncryptedFile/unprotector.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),
 UnprotectCustomEncryptedFile_P("Input/error/error.pdf","OutPut/UnprotectCustomEncryptedFile/error.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),
 UnprotectCustomEncryptedFile_P("Input/Protector/password.pdf","OutPut/UnprotectCustomEncryptedFile/password.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),
 UnprotectCustomEncryptedFile_P("Input/Protector/OfficeTemplate.ppdf","OutPut/UnprotectCustomEncryptedFile/OfficeTemplate.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_FORMAT),//Protector.h中的加密ppdf走的是另外路线，
 UnprotectCustomEncryptedFile_P("Input/Protector/MaxOwner.pdf","OutPut/UnprotectCustomEncryptedFile/MaxOwner.pdf",PDF_PROTECTOR_FILTER_NAME,"",PDFCREATOR_ERR_SUCCESS),
 UnprotectCustomEncryptedFile_P("Input/Protector/phantomOfficeT.pdf","OutPut/UnprotectCustomEncryptedFile/anyone.pdf",PDF_PROTECTOR_FILTER_NAME,"Only the owner has the right to unprotect the document.",PDFCREATOR_ERR_UNKNOWN)
                            ));
struct GetWrapperType_P{
    GetWrapperType_P(std::string fileIn_,
             std::string Exceptions_,
             uint32_t ret_)
    {
        fileIn = fileIn_;
        ExceptionsMess=Exceptions_;
        ret = ret_;
    }
    std::string fileIn;
    std::string ExceptionsMess;
    uint32_t ret;
};

class PDFWrapperDoc_GetWrapperType:public ::testing::TestWithParam<GetWrapperType_P>
{

};
TEST_P(PDFWrapperDoc_GetWrapperType,GetWrapperType_T)
{
    GetWrapperType_P TParam=GetParam();
    PDFModuleMgr::Initialize();
    std::string fileIn= GetCurrentInputFile()+TParam.fileIn;

    auto inFile = make_shared<fstream>(fileIn, ios_base::in | ios_base::out | ios_base::binary);
    std::shared_ptr<std::iostream> inputEncryptedIO = inFile;
    auto inputEncrypted = rmscrypto::api::CreateStreamFromStdStream(inputEncryptedIO);
    uint32_t ret;
    try{
    std::unique_ptr<PDFWrapperDoc> pdfWrapperDoc =  PDFWrapperDoc::Create(inputEncrypted);

    ret =pdfWrapperDoc->GetWrapperType();
    }
    catch(const rmsauth::Exception& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionsMess,message);
        return;
    }
    EXPECT_EQ(TParam.ret,ret);
}
INSTANTIATE_TEST_CASE_P(,PDFWrapperDoc_GetWrapperType,testing::Values(
GetWrapperType_P("Input/Protector/anyone.pdf","No Exception",2),
GetWrapperType_P("Input/Protector/V1V1.pdf","No Exception",1),//V1
GetWrapperType_P("Input/Protector/OfficeTemplate.ppdf","No Exception",0),
GetWrapperType_P("Input/Protector/V1V0.pdf","No Exception",2),
GetWrapperType_P("Input/Protector/V0V0.pdf","No Exception",2),
GetWrapperType_P("Input/Test.txt","No Exception",0),
GetWrapperType_P("Input/Protector/cer.pdf","No Exception",0),
GetWrapperType_P("Input/Protector/password.pdf","No Exception",0),
GetWrapperType_P("Input/Errorr/Error.pdf","No Exception",0),
GetWrapperType_P("Input/unprotector.pdf","No Exception",0)
));
struct GetCryptographicFilter_P{
    GetCryptographicFilter_P(std::string fileIn_,
    std::wstring wsGraphicFilter_,
    float fVersion_,
    bool ret_,
    std::string ExceptionsMess_)
    {
        fileIn =fileIn_;
        wsGraphicFilter = wsGraphicFilter_;
        fVersion =fVersion_;
        ret = ret_;
        ExceptionsMess = ExceptionsMess_;
    }
    std::string fileIn;
    std::wstring wsGraphicFilter;
    float fVersion;
    bool ret;
    std::string ExceptionsMess;
};
class PDFWrapperDoc_GetCryptographicFilter:public ::testing::TestWithParam<GetCryptographicFilter_P>
{

};
TEST_P(PDFWrapperDoc_GetCryptographicFilter,GetCryptographicFilter_T)
{
    GetCryptographicFilter_P TParam=GetParam();
    PDFModuleMgr::Initialize();
    std::string fileIn= GetCurrentInputFile()+TParam.fileIn;

    auto inFile = make_shared<fstream>(fileIn, ios_base::in | ios_base::out | ios_base::binary);
    std::shared_ptr<std::iostream> inputEncryptedIO = inFile;
    auto inputEncrypted = rmscrypto::api::CreateStreamFromStdStream(inputEncryptedIO);
    bool ret;
    std::wstring wsGraphicFilter;
    float fVersion;
    try{
    std::unique_ptr<PDFWrapperDoc> pdfWrapperDoc =  PDFWrapperDoc::Create(inputEncrypted);

    ret =pdfWrapperDoc->GetCryptographicFilter(wsGraphicFilter,fVersion);
    }
    catch(const rmsauth::Exception& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionsMess,message);
        return;
    }
    EXPECT_EQ(TParam.ret,ret);
    if(TParam.ret == true)
    {
        EXPECT_EQ(TParam.wsGraphicFilter,wsGraphicFilter);
        EXPECT_EQ(TParam.fVersion,fVersion);
    }

}
INSTANTIATE_TEST_CASE_P(,PDFWrapperDoc_GetCryptographicFilter,testing::Values(
GetCryptographicFilter_P("Input/Protector/OfficeTemplate.ppdf",L"",0,true,"NO Exception"),//
GetCryptographicFilter_P("Input/Protector/test.pdf",L"Test",7,true,"NO Exception"),//自己设置的值，Test,7
GetCryptographicFilter_P("Input/Protector/password.pdf",L"",0,true,"NO Exception"),
GetCryptographicFilter_P("Input/Protector/error.pdf",L"",0,true,"NO Exception"),
GetCryptographicFilter_P("Input/Protector/customerTemplate.pdf",L"FoxitRMSV2",1,true,"NO Exception"),
GetCryptographicFilter_P("Input/unprotector.pdf",L"",0,true,"NO Exception"),
GetCryptographicFilter_P("Input/Protector/V1V1.pdf",L"MicrosoftIRMServices",1,true,"NO Exception"),
GetCryptographicFilter_P("Input/Protector/anyone.pdf",L"MicrosoftIRMServices",2,true,"NO Exception")


));
struct GetPayLoadSize_P{
    GetPayLoadSize_P(std::string fileIn_,
    uint32_t PayLoadSize_,
    std::string ExceptionMess_)
    {
        fileIn =fileIn_;
        PayLoadSize = PayLoadSize_;
        ExceptionMess = ExceptionMess_;
    }
    std::string fileIn;
    uint32_t PayLoadSize;
    std::string ExceptionMess;

};
class PDFWrapperDoc_GetPayLoadSize:public ::testing::TestWithParam<GetPayLoadSize_P>
{

};
TEST_P(PDFWrapperDoc_GetPayLoadSize,GetPayLoadSize_T)
{
    GetPayLoadSize_P TParam=GetParam();
    PDFModuleMgr::Initialize();
    std::string fileIn= GetCurrentInputFile()+TParam.fileIn;

    auto inFile = make_shared<fstream>(fileIn, ios_base::in | ios_base::out | ios_base::binary);
    std::shared_ptr<std::iostream> inputEncryptedIO = inFile;
    auto inputEncrypted = rmscrypto::api::CreateStreamFromStdStream(inputEncryptedIO);
    uint32_t PayLoadSize;
    try{
    std::unique_ptr<PDFWrapperDoc> pdfWrapperDoc =  PDFWrapperDoc::Create(inputEncrypted);

    PayLoadSize =pdfWrapperDoc->GetPayLoadSize();
    }
    catch(const rmsauth::Exception& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionMess,message);
        return;
    }
    EXPECT_EQ(TParam.PayLoadSize,PayLoadSize);
}

INSTANTIATE_TEST_CASE_P(,PDFWrapperDoc_GetPayLoadSize,testing::Values(
GetPayLoadSize_P("Input/ERROR/Error.pdf",0,""),
GetPayLoadSize_P("Input/Protector/Test.txt",0,""),
GetPayLoadSize_P("Input/Protector/cer.pdf",0,""),
GetPayLoadSize_P("Input/Protector/password.pdf",0,""),
GetPayLoadSize_P("Input/Protector/OfficeTemplate.ppdf",0,""),
GetPayLoadSize_P("Input/unprotector.pdf",0,""),
GetPayLoadSize_P("Input/Protector/customerTemplate.pdf",26488,""),
GetPayLoadSize_P("Input/Protector/V1V1.pdf",55728,""),
GetPayLoadSize_P("Input/Protector/anyone.pdf",25699,"")
));
struct GetPayloadFileName_P{
    GetPayloadFileName_P(std::string fileIn_,
    std::wstring wsFileName_,
    bool ret_,
    std::string ExceptionMess_)
    {
        fileIn = fileIn_;
        wsFileName = wsFileName_;
        ret = ret_;
        ExceptionMess = ExceptionMess_;
    }
    std::string fileIn;
    std::wstring wsFileName;
    bool ret;
    std::string ExceptionMess;
};
class PDFWrapperDoc_GetPayloadFileName:public ::testing::TestWithParam<GetPayloadFileName_P>
{

};
TEST_P(PDFWrapperDoc_GetPayloadFileName,GetPayloadFileName_T)
{
    GetPayloadFileName_P TParam=GetParam();
    PDFModuleMgr::Initialize();
    std::string fileIn= GetCurrentInputFile()+TParam.fileIn;

    auto inFile = make_shared<fstream>(fileIn, ios_base::in | ios_base::out | ios_base::binary);
    std::shared_ptr<std::iostream> inputEncryptedIO = inFile;
    auto inputEncrypted = rmscrypto::api::CreateStreamFromStdStream(inputEncryptedIO);
    bool ret;
    std::wstring wsFileName;
    try{
        std::unique_ptr<PDFWrapperDoc> pdfWrapperDoc =  PDFWrapperDoc::Create(inputEncrypted);
        ret =pdfWrapperDoc->GetPayloadFileName(wsFileName);
    }
    catch(const rmsauth::Exception& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionMess,message);
        return;
    }
    EXPECT_EQ(TParam.ret,ret);
    if(TParam.ret == true)
    {
        EXPECT_EQ(TParam.wsFileName,wsFileName);
    }
}
INSTANTIATE_TEST_CASE_P(,PDFWrapperDoc_GetPayloadFileName,testing::Values(
GetPayloadFileName_P("Input/Protector/customerTemplate.pdf",L"",true,""),
GetPayloadFileName_P("Input/Protector/OfficeTemplate.ppdf",L"",true,""),
GetPayloadFileName_P("Input/Protector/password.pdf",L"",true,""),
GetPayloadFileName_P("Input/Protector/test.pdf",L"Test",true,""),
GetPayloadFileName_P("Input/Protector/V1V1.pdf",L"",true,""),//V1
GetPayloadFileName_P("Input/Protector/cer.pdf",L"",true,""),
GetPayloadFileName_P("Input/Protector/anyone.pdf",L"MicrosoftIRMServices Protected PDF.pdf",true,"")
));
struct CreateUnencryptedWrapper_P{
    CreateUnencryptedWrapper_P(std::string fileIn_,
                              // std::string fileOut_,
                               bool ret_,
                               std::string ExceptionMess_)
    {
        fileIn = fileIn_;
        //fileout = fileOut_;
        ret = ret_;
        ExceptionMess= ExceptionMess_;
    }
    std::string fileIn;
    //std::string fileout;
    bool ret;
    std::string ExceptionMess;
};

class PDFUnencryptedWrapperCreator_CreateUnencryptedWrapper:public::testing::TestWithParam<CreateUnencryptedWrapper_P>
{

};

TEST_P(PDFUnencryptedWrapperCreator_CreateUnencryptedWrapper,CreateUnencryptedWrapper_T)
{
    CreateUnencryptedWrapper_P TParam=GetParam();
    PDFModuleMgr::Initialize();
    std::string fileIn= GetCurrentInputFile()+TParam.fileIn;
    auto inFile = make_shared<fstream>(fileIn, ios_base::in | ios_base::out | ios_base::binary);
    string fileOut = "OutPut/CreateUnencryptedWrapper/anyone.pdf";
    auto outFile = make_shared<fstream>(fileOut, ios_base::in | ios_base::out | ios_base::trunc | ios_base::binary);
    std::shared_ptr<std::iostream> inputEncryptedIO = inFile;
    auto inputEncrypted = rmscrypto::api::CreateStreamFromStdStream(inputEncryptedIO);
    uint32_t ret;
    try{
        std::unique_ptr<PDFUnencryptedWrapperCreator> pdfWrapperDoc =  PDFUnencryptedWrapperCreator::Create(inputEncrypted);
        std::shared_ptr<std::iostream> inputEncryptedIO = outFile;
        auto outputStream = rmscrypto::api::CreateStreamFromStdStream(inputEncryptedIO);
        ret =pdfWrapperDoc->CreateUnencryptedWrapper(outputStream);
    }
    catch(const rmsauth::Exception& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionMess,message);
        return;
    }
    EXPECT_EQ(TParam.ret,ret);
};
INSTANTIATE_TEST_CASE_P(,PDFUnencryptedWrapperCreator_CreateUnencryptedWrapper,testing::Values(
CreateUnencryptedWrapper_P("Input/XFAStatic.pdf",1,""),
CreateUnencryptedWrapper_P("Input/XFADyanmic-crash.pdf",1,""),
CreateUnencryptedWrapper_P("Input/sign.pdf",1,""),
CreateUnencryptedWrapper_P("Input/Protector/pwd_protect.pdf",1,""),
CreateUnencryptedWrapper_P("Input/ERROR/Error.pdf",0,""),
CreateUnencryptedWrapper_P("Input/Protector/cer.pdf",0,""),
CreateUnencryptedWrapper_P("Input/Protector/password.pdf",0,""),
CreateUnencryptedWrapper_P("Input/Protector/customerTemplate.pdf",1,""),
CreateUnencryptedWrapper_P("Input/Protector/V1V1.pdf",1,""),
CreateUnencryptedWrapper_P("Input/Protector/OfficeTemplate.ppdf",0,""),
CreateUnencryptedWrapper_P("Input/unprotector.pdf",1,""),
CreateUnencryptedWrapper_P("Input/demage.pdf",0,""),
CreateUnencryptedWrapper_P("Input/Test.txt",0,"")

                            ));

struct SetPayloadInfo_P{
    SetPayloadInfo_P(std::wstring wsSubType_,
                     std::wstring wsFileName_,
                     std::wstring wsDescription_,
                     std::string outfile_,
                     float fVersion_,
                     std::string ExceptionMess_)
    {
        wsSubType = wsSubType_;
        wsFileName= wsFileName_;
        wsDescription = wsDescription_;
        fileout = outfile_;
        fVersion = fVersion_;
        ExceptionsMess = ExceptionMess_;
    }
    std::wstring wsSubType;
    std::wstring wsFileName;
    std::wstring wsDescription;
    std::string fileout;
    float fVersion;
    std::string ExceptionsMess;
};
class PDFUnencryptedWrapperCreator_SetPayloadInfo:public::testing::TestWithParam<SetPayloadInfo_P>
{
public:
 static void SetUpTestCase() {

     SetUserPolicy();
 }
 static void TearDownTestCase() {

 }
};

TEST_P(PDFUnencryptedWrapperCreator_SetPayloadInfo,SetPayloadInfo_T)
{
    SetPayloadInfo_P TParam= GetParam();
    std::string fileIn=GetCurrentInputFile() +"Input/unprotector.pdf";


    auto inFile = make_shared<fstream>(
      fileIn, ios_base::in | ios_base::out | ios_base::binary);

    std::unique_ptr<PDFCreator> m_pdfCreator = PDFCreator::Create();

    auto encryptedSS = std::make_shared<std::stringstream>();
    std::shared_ptr<std::iostream> encryptedIOS = encryptedSS;
    auto outputEncrypted = rmscrypto::api::CreateStreamFromStdStream(encryptedIOS);

    std::string originalFileExtension=".pFile";

    auto p_PDFprotector = std::make_shared<PDFProtector_unit>(fileIn,originalFileExtension,inFile);
    auto cryptoHander = std::make_shared<PDFCryptoHandler_child>(p_PDFprotector);

    p_PDFprotector->SetUserPolicy(m_userPolicy);

    std::string filterName = PDF_PROTECTOR_FILTER_NAME;
    std::vector<unsigned char> publishingLicense = m_userPolicy->SerializedPolicy();

    uint32_t ret;
    ret = m_pdfCreator->CreateCustomEncryptedFile(fileIn,
                                                  filterName,
                                                  publishingLicense,
                                                  cryptoHander,
                                                  outputEncrypted);

    std::string wrapperIn= GetCurrentInputFile() +"Input/wrapper.pdf";


    auto inwrapper = make_shared<fstream>(
      wrapperIn, ios_base::in | ios_base::out | ios_base::binary);
    std::shared_ptr<std::iostream> inputWrapperIO = inwrapper;
    auto inputWrapper = rmscrypto::api::CreateStreamFromStdStream(inputWrapperIO);
    try{
        std::unique_ptr<PDFUnencryptedWrapperCreator> m_pdfWrapperCreator=PDFUnencryptedWrapperCreator::Create(inputWrapper);
        m_pdfWrapperCreator = PDFUnencryptedWrapperCreator::Create(inputWrapper);
        m_pdfWrapperCreator->SetPayloadInfo(
                    TParam.wsSubType,
                    TParam.wsFileName,
                    TParam.wsDescription,
                    TParam.fVersion);
        m_pdfWrapperCreator->SetPayLoad(outputEncrypted);

        std::string fileOut =GetCurrentInputFile()+TParam.fileout;
        auto outputStream = make_shared<fstream>(
          fileOut, ios_base::in | ios_base::out | ios_base::trunc | ios_base::binary);
        std::shared_ptr<std::iostream> outputIO = outputStream;
        auto outputWrapper = rmscrypto::api::CreateStreamFromStdStream(outputIO);
       m_pdfWrapperCreator->CreateUnencryptedWrapper(outputWrapper);
       //**************************
       auto inpro = make_shared<fstream>(fileOut, ios_base::in | ios_base::out | ios_base::binary);
       std::shared_ptr<std::iostream> inputEncryptedIO = inpro;
       auto inputEncrypted = rmscrypto::api::CreateStreamFromStdStream(inputEncryptedIO);
       bool ret;
       std::wstring wsFileName;
       std::unique_ptr<PDFWrapperDoc> pdfWrapperDoc =  PDFWrapperDoc::Create(inputEncrypted);
       pdfWrapperDoc->GetPayloadFileName(wsFileName);

       std::wstring wsGraphicFilter;
       float fVersion;
       pdfWrapperDoc->GetCryptographicFilter(wsGraphicFilter,fVersion);
       if(!(TParam.wsFileName.compare(L" ")))
       {
           EXPECT_EQ(TParam.wsSubType,wsGraphicFilter);
           EXPECT_EQ(L"Embedded File",wsFileName);
           EXPECT_EQ(TParam.fVersion,fVersion);
       }
       else
       {
           EXPECT_EQ(TParam.wsSubType,wsGraphicFilter);
           EXPECT_EQ(TParam.wsFileName,wsFileName);
           EXPECT_EQ(TParam.fVersion,fVersion);
       }

    }
    catch (const rmsauth::Exception& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionsMess,message);
        return;
    }
    catch (const rmscore::exceptions::RMSPDFFileException& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionsMess,message);
        return;
    }
    catch (const rmscore::exceptions::RMSException& e)
    {
        std::string message(e.what());
        EXPECT_EQ(TParam.ExceptionsMess,message);
        return;
    }

}
INSTANTIATE_TEST_CASE_P(,PDFUnencryptedWrapperCreator_SetPayloadInfo,testing::Values(
SetPayloadInfo_P(L" ",L" ",L" ","/OutPut/SetPayloadInfo/nullvalue.pdf",1,""),//
SetPayloadInfo_P(L"test",L"test",L"test","/OutPut/SetPayloadInfo/China.pdf",1,""),
SetPayloadInfo_P(L"MicrosoftIRMServices",L"MicrosoftIRMServices",L"This embedded file is encrypted using MicrosoftIRMServices filter","/OutPut/SetPayloadInfo/normal.pdf",2,""),
SetPayloadInfo_P(L"Test",L"Test",L"Test","/OutPut/SetPayloadInfo/test.pdf",7,"")
));
