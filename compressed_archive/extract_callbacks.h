#ifndef EXTRACT_CALLBACKS_H
#define EXTRACT_CALLBACKS_H

#include "7z_includes.h"
#include "extract_delegate.h"

#include <QDebug>

using namespace NWindows;

//////////////////////////////////////////////////////////////
// Archive Extracting callback class

static const wchar_t *kCantDeleteOutputFile = L"ERROR: Can not delete output file ";

static const char *kTestingString = "Testing     ";
static const char *kExtractingString = "Extracting  ";
static const char *kSkippingString = "Skipping    ";

static const char *kUnsupportedMethod = "Unsupported Method";
static const char *kCRCFailed = "CRC Failed";
static const char *kDataError = "Data Error";
static const char *kUnknownError = "Unknown Error";

static const wchar_t *kEmptyFileAlias = L"[Content]";

static HRESULT IsArchiveItemProp(IInArchive *archive, UInt32 index, PROPID propID, bool &result)
{
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, propID, &prop));
    if (prop.vt == VT_BOOL)
        result = VARIANT_BOOLToBool(prop.boolVal);
    else if (prop.vt == VT_EMPTY)
        result = false;
    else
        return E_FAIL;
    return S_OK;
}
static HRESULT IsArchiveItemFolder(IInArchive *archive, UInt32 index, bool &result)
{
    return IsArchiveItemProp(archive, index, kpidIsDir, result);
}

class YCArchiveExtractCallback : public IArchiveExtractCallback,
                                 public ICryptoGetTextPassword,
                                 public CMyUnknownImp
{
    Z7_IFACES_IMP_UNK_2(IArchiveExtractCallback, ICryptoGetTextPassword)
    Z7_IFACE_COM7_IMP(IProgress)

private:
    CMyComPtr<IInArchive> _archiveHandler;
    UString _directoryPath; // Output directory
    UString _filePath; // name inside arcvhive
    UString _diskFilePath; // full path to file on disk
    bool _extractMode;
    bool all;
    ExtractDelegate *delegate;
    UInt32 _index;
    struct CProcessedFileInfo {
        FILETIME MTime;
        UInt32 Attrib;
        bool isDir;
        bool AttribDefined;
        bool MTimeDefined;
    } _processedFileInfo;

    COutFileStream *_outFileStreamSpec;
    CMyComPtr<ISequentialOutStream> _outFileStream;

public:
    void Init(IInArchive *archiveHandler, const UString &directoryPath);

    UInt64 NumErrors;
    bool PasswordIsDefined;
    QList<QByteArray> allFiles;
    UString Password;
    Byte *data;
    UInt64 newFileSize;
    // false as soon as any entry fails to extract, so callers can tell a real
    // page from whatever happened to be left in the buffer
    bool lastExtractionOk;
    QMap<qint32, qint32> indexesToPages;

    // A single page larger than this is a corrupt size field, not a comic page.
    // Trusting it would hand MidAlloc/QByteArray an absurd allocation.
    static constexpr UInt64 kMaxEntrySize = 1024ULL * 1024 * 1024;

    YCArchiveExtractCallback(const QMap<qint32, qint32> &indexesToPages, bool c = false, ExtractDelegate *d = 0)
        : all(c), delegate(d), NumErrors(0), PasswordIsDefined(false), data(nullptr), newFileSize(0), lastExtractionOk(false), indexesToPages(indexesToPages) { }
    ~YCArchiveExtractCallback()
    {
        MidFree(data);
        data = nullptr;
    }

private:
    void releaseBuffer()
    {
        MidFree(data);
        data = nullptr;
        newFileSize = 0;
    }
};

void YCArchiveExtractCallback::Init(IInArchive *archiveHandler, const UString &directoryPath)
{
    NumErrors = 0;
    _archiveHandler = archiveHandler;
    _extractMode = false;
    _index = 0;
    _processedFileInfo.Attrib = 0;
    _processedFileInfo.isDir = false;
    _processedFileInfo.AttribDefined = false;
    _processedFileInfo.MTimeDefined = false;
    lastExtractionOk = false;
    releaseBuffer();
    directoryPath; // unused
}

Z7_COM7F_IMF(YCArchiveExtractCallback::SetTotal(UInt64 /* size */))
{
    return S_OK;
}

Z7_COM7F_IMF(YCArchiveExtractCallback::SetCompleted(const UInt64 * /* completeValue */))
{
    return S_OK;
}

Z7_COM7F_IMF(YCArchiveExtractCallback::GetStream(UInt32 index,
                                                 ISequentialOutStream **outStream, Int32 askExtractMode))
{
    *outStream = 0;
    _outFileStream.Release();

    if (indexesToPages.isEmpty())
        _index = index;
    else
        _index = indexesToPages.value(index);

    {
        // Get Name
        NCOM::CPropVariant prop;
        RINOK(_archiveHandler->GetProperty(index, kpidPath, &prop));

        UString fullPath;
        if (prop.vt == VT_EMPTY)
            fullPath = kEmptyFileAlias;
        else {
            if (prop.vt != VT_BSTR)
                return E_FAIL;
            fullPath = prop.bstrVal;
        }
        _filePath = fullPath;
    }

    askExtractMode; // unused
    // if (askExtractMode != NArchive::NExtract::NAskMode::kExtract)
    // return S_OK;

    {
        // Get Attrib
        NCOM::CPropVariant prop;
        RINOK(_archiveHandler->GetProperty(index, kpidAttrib, &prop));
        if (prop.vt == VT_EMPTY) {
            _processedFileInfo.Attrib = 0;
            _processedFileInfo.AttribDefined = false;
        } else {
            if (prop.vt != VT_UI4)
                return E_FAIL;
            _processedFileInfo.Attrib = prop.ulVal;
            _processedFileInfo.AttribDefined = true;
        }
    }

    RINOK(IsArchiveItemFolder(_archiveHandler, index, _processedFileInfo.isDir));

    {
        // Get Modified Time
        NCOM::CPropVariant prop;
        RINOK(_archiveHandler->GetProperty(index, kpidMTime, &prop));
        _processedFileInfo.MTimeDefined = false;
        switch (prop.vt) {
        case VT_EMPTY:
            // _processedFileInfo.MTime = _utcMTimeDefault;
            break;
        case VT_FILETIME:
            _processedFileInfo.MTime = prop.filetime;
            _processedFileInfo.MTimeDefined = true;
            break;
        default:
            return E_FAIL;
        }
    }

    // se necesita conocer el tama?o del archivo para poder reservar suficiente memoria
    bool newFileSizeDefined;
    {
        // Get Size
        NCOM::CPropVariant prop;
        RINOK(_archiveHandler->GetProperty(index, kpidSize, &prop));
        newFileSizeDefined = (prop.vt != VT_EMPTY);
        if (newFileSizeDefined)
            ConvertPropVariantToUInt64(prop, newFileSize);
    }

    // No hay que crear ning?n fichero, ni directorios intermedios
    /*{
    // Create folders for file
    int slashPos = _filePath.ReverseFind(WCHAR_PATH_SEPARATOR);
    if (slashPos >= 0)
      NFile::NDirectory::CreateComplexDirectory(_directoryPath + _filePath.Left(slashPos));
  }

  UString fullProcessedPath = _directoryPath + _filePath;
  _diskFilePath = fullProcessedPath;
  */
    if (_processedFileInfo.isDir) {
        // NFile::NDirectory::CreateComplexDirectory(fullProcessedPath);
    } else {
        /*NFile::NFind::CFileInfoW fi;
      if (fi.Find(fullProcessedPath))
      {
      if (!NFile::NDirectory::DeleteFileAlways(fullProcessedPath))
      {
      qDebug() <<(UString(kCantDeleteOutputFile) + fullProcessedPath);
      return E_ABORT;
      }
      }*/
        if (newFileSizeDefined) {
            // Whatever the previous entry left behind is dead now. Without this the
            // buffer of every failed entry leaks, because only the success path frees it.
            const UInt64 wantedSize = newFileSize;
            releaseBuffer();

            if (wantedSize > kMaxEntrySize) {
                qDebug() << "Refusing to extract entry with implausible size" << wantedSize;
                return E_OUTOFMEMORY;
            }

            data = (Byte *)MidAlloc(wantedSize);
            if (data == nullptr) {
                qDebug() << "Out of memory extracting entry of size" << wantedSize;
                return E_OUTOFMEMORY;
            }
            newFileSize = wantedSize;

            CBufPtrSeqOutStream *outStreamSpec = new CBufPtrSeqOutStream;
            CMyComPtr<ISequentialOutStream> outStreamLocal(outStreamSpec);
            outStreamSpec->Init(data, newFileSize);
            *outStream = outStreamLocal.Detach();
        } else {
            // No size means no buffer, so make sure a stale one is not mistaken for this entry
            releaseBuffer();
        }
    }
    return S_OK;
}

Z7_COM7F_IMF(YCArchiveExtractCallback::PrepareOperation(Int32 askExtractMode))
{
    _extractMode = false;
    switch (askExtractMode) {
    case NArchive::NExtract::NAskMode::kExtract:
        _extractMode = true;
        break;
    };
    /* switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract:  qDebug() << (kExtractingString); break;
    case NArchive::NExtract::NAskMode::kTest:  qDebug() <<(kTestingString); break;
    case NArchive::NExtract::NAskMode::kSkip:  qDebug() <<(kSkippingString); break;
  };*/
    // qDebug() << _filePath;
    return S_OK;
}

Z7_COM7F_IMF(YCArchiveExtractCallback::SetOperationResult(Int32 operationResult))
{
    switch (operationResult) {
    case NArchive::NExtract::NOperationResult::kOK:
        lastExtractionOk = (data != nullptr) || _processedFileInfo.isDir;
        if (all && !_processedFileInfo.isDir) {
            QByteArray rawData;
            if (data != nullptr)
                rawData = QByteArray((char *)data, static_cast<qsizetype>(newFileSize));
            releaseBuffer();
            if (delegate != 0)
                delegate->fileExtracted(_index, rawData);
            else {
                allFiles.append(rawData);
            }
        }
        break;
    default: {
        NumErrors++;
        lastExtractionOk = false;
        // The buffer holds a partially decompressed page at best. Drop it so no
        // caller can mistake it for a good one, and so it is not leaked.
        releaseBuffer();
        qDebug() << "     ";
        switch (operationResult) {
        case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
            if (delegate != 0)
                delegate->unknownError(_index);
            qDebug() << kUnsupportedMethod;
            break;
        case NArchive::NExtract::NOperationResult::kCRCError:
            if (delegate != 0)
                delegate->crcError(_index);
            qDebug() << kCRCFailed;
            break;
        case NArchive::NExtract::NOperationResult::kDataError:
            if (delegate != 0)
                delegate->unknownError(_index);
            qDebug() << kDataError;
            break;
        default:
            if (delegate != 0)
                delegate->unknownError(_index);
            qDebug() << kUnknownError;
        }
    }
    }
    /*
  if (_outFileStream != NULL)
  {
    if (_processedFileInfo.MTimeDefined)
      _outFileStreamSpec->SetMTime(&_processedFileInfo.MTime);
    RINOK(_outFileStreamSpec->Close());
  }
  _outFileStream.Release();
  if (_extractMode && _processedFileInfo.AttribDefined)
    NFile::NDirectory::MySetFileAttributes(_diskFilePath, _processedFileInfo.Attrib);*/
    // qDebug() << endl;
    return S_OK;
}

Z7_COM7F_IMF(YCArchiveExtractCallback::CryptoGetTextPassword(BSTR *password))
{
    if (!PasswordIsDefined) {
        // You can ask real password here from user
        // Password = GetPassword(OutStream);
        // PasswordIsDefined = true;
        qDebug() << "Password is not defined" << Qt::endl;
        return E_ABORT;
    }
    return StringToBstr(Password, password);
}

#endif
