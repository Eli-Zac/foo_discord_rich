#include <stdafx.h>

#include "uploader.h"

#include <fb2k/config.h>

#include <ctime>
#include <fb2k/artwork_metadb.h>

#include "discord_impl.h"
#include "image_hasher.h"
#include "foobar2000/SDK/component.h"

namespace drp::uploader
{
/**
 * Used to restrict uploading to one image at a time.
 * used by defining a variable of this type. Lock is automatically acquired,
 * and it is unlocked after the variable goes out of scope
 */
class upload_lock
{
public:
    upload_lock()
    {
        lock_.lock();
    }
    ~upload_lock()
    {
        lock_.unlock();
    }

    static bool is_locked()
    {
        const auto locked = lock_.try_lock();
        if ( locked )
        {
            lock_.unlock();
        }
        return !locked;
    }

private:
    static std::mutex lock_;
};

// Must initialize static class variables outside of the class
std::mutex upload_lock::lock_;

static bool hasArtworkData(const artwork_info& art)
{
    return art.data.is_valid();
}

static bool hasArtworkSource(const artwork_info& art)
{
    return strlen(art.path) > 0 || hasArtworkData(art);
}

static void ensureArtworkHash(artwork_info& art)
{
    if (art.artwork_hash.get_length() > 0 || !hasArtworkData(art))
    {
        return;
    }

    art.artwork_hash = static_api_ptr_t<hasher_md5>()
        ->process_single(art.data->get_ptr(), art.data->get_size())
        .asString();
}

threaded_process_artwork_uploader::threaded_process_artwork_uploader(
    const pfc::map_t<metadb_index_hash, metadb_handle_ptr>& hashes, const bool regenerate) : hashes_(hashes), regenerate_(regenerate)
{}

void threaded_process_artwork_uploader::on_init(ctx_t p_wnd) {}
    
void threaded_process_artwork_uploader::run(threaded_process_status &p_status, abort_callback &p_abort)
{
    pfc::list_t<metadb_index_hash> lstChanged; // Linear list of hashes that actually changed
    const auto total_count = hashes_.get_count();
    t_size currIdx = 0;
    
    for (auto iter = hashes_.first(); iter.is_valid(); ++iter) {
        try
        {
            p_status.set_progress_float( currIdx / (double)total_count );
            const auto kv = *iter;
            
            if (!regenerate_ && record_get( kv.m_key ).artwork_url.get_length() > 0)
            {
                p_abort.check();
                currIdx++;
                continue;
            }

            pfc::string8 artwork_url;

            if (extractAndUploadArtwork(kv.m_value, p_abort, artwork_url, kv.m_key, regenerate_))
            {
                lstChanged += kv.m_key;
            }
            
            p_abort.check();
        }
        catch (exception_aborted)
        {
            return;
        }
        currIdx++;
    }

    p_status.set_progress_float(1);
    FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": " << lstChanged.get_count() << " entries updated";


    if (lstChanged.get_count() > 0) {
        // This gracefully tells everyone about what just changed, in one pass regardless of how many items got altered
        fb2k::inMainThread([lstChanged]
        {
            cached_index_api()->dispatch_refresh(guid::artwork_url_index, lstChanged);
            DiscordHandler::GetInstance().GetPresenceModifier().UpdateImage();
        });
    }
}

void threaded_process_artwork_uploader::on_done(ctx_t p_wnd,bool p_was_aborted)
{
}

bool extractAndUploadArtwork(const metadb_handle_ptr track, abort_callback &abort, pfc::string8 &artwork_url, metadb_index_hash hash, const bool regenerate)
{
    if ( config::uploadArtworkCommand.GetValue().length() == 0 )
    {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": No upload command given";
        return false;
    }

    bool wasLocked = upload_lock::is_locked();
    upload_lock lock;
    abort.check();

    // If we were locked check if this tracks artwork was uploaded and use that if it is found.
    // If the artwork url needs to be regenerated do not do this
    if (wasLocked && !regenerate)
    {
        const auto rec = record_get( hash );
        if (rec.artwork_url.get_length() > 0)
        {
            artwork_url = rec.artwork_url;
            return true;
        }
    }

    auto artwork = extractArtwork( track, abort );
    abort.check();
    if (artwork.success)
    {
#ifdef _DEBUG
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Artwork path after extract " << artwork.path;
#endif
        artwork_url = uploadArtwork( artwork, abort, hash );
        if (artwork_url.get_length() > 0)
        {
            drp::artwork_url_set( hash, artwork_url );
            return true;
        }
    }
    else if (usesUrlPlaceholder(config::uploadArtworkCommand.GetValue()))
    {
        // Artwork extraction failed but command uses {url} placeholder, so proceed with uploadArtwork
        artwork_url = uploadArtwork( artwork, abort, hash );
        if (artwork_url.get_length() > 0)
        {
            drp::artwork_url_set( hash, artwork_url );
            return true;
        }
    }

    return false;
}

artwork_info extractArtwork( const metadb_handle_ptr track, abort_callback &abort )
{
    auto aam = album_art_manager_v3::get();
    auto extractor = aam->open(pfc::list_single_ref_t(track),
                           pfc::list_single_ref_t(album_art_ids::cover_front), abort);

    if (!extractor.is_valid())
    {
        #ifdef _DEBUG
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Invalid artwork extractor instance found";
        #endif
        return artwork_info();
    }

    try {
        abort.check();
        artwork_info info;

        const auto paths = extractor->query_paths( album_art_ids::cover_front, abort );
        if (paths->get_count() > 0)
        {
            info.path = pfc::string8(paths->get_path(0));
            auto loc = track->get_location().get_path();

            #ifdef _DEBUG
            FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": extracted filepath " << info.path << " track filepath " << loc << " is valid? " << paths.is_valid();
            #endif
            // Artwork location same as file means artwork is embedded
            if (info.path.equals(loc))
            {
                info.path = "";
            }
        }
        else
        {
            info.path = "";
        }
        
        info.data = extractor->query(album_art_ids::cover_front, abort);
        info.success = true;

        #ifdef _DEBUG
        if (info.path != "")
        {
            FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": found existing path " << info.path;
        }
        #endif
        return info;
    } catch (const exception_album_art_not_found&) {
        return artwork_info();
    } catch (const exception_aborted&) {
        throw;
    } catch (...) {
        return artwork_info();
    }
}

/**
 * Initialize different process variables to be used with CreateProcess
 */
bool initializeProcessVariables(
    HANDLE &g_hChildStd_IN_Rd,
    HANDLE &g_hChildStd_IN_Wr,
    HANDLE &g_hChildStd_OUT_Rd,
    HANDLE &g_hChildStd_OUT_Wr,
    SECURITY_ATTRIBUTES &saAttr)
{
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if ( !CreatePipe( &g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0 ) )
        return false;

    // Ensure the read handle to the pipe for STDOUT is not inherited.

    if ( !SetHandleInformation( g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0 ) )
        return false;

    // Create a pipe for the child process's STDIN.

    if ( !CreatePipe( &g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0 ) )
        return false;

    // Ensure the write handle to the pipe for STDIN is not inherited.

    if ( !SetHandleInformation( g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0 ) )
        return false;

    return true;
}

void closePipes(HANDLE pipe1, HANDLE pipe2, HANDLE pipe3, HANDLE pipe4)
{
    if (pipe1 != NULL) CloseHandle(pipe1);
    if (pipe2 != NULL) CloseHandle(pipe2);
    if (pipe3 != NULL) CloseHandle(pipe3);
    if (pipe4 != NULL) CloseHandle(pipe4);
}

bool readFromPipe(HANDLE g_hChildStd_OUT_Rd, pfc::string8 &artwork_url)
{
    bool inputFound = false;
    const int TEMP_BUF_SIZE = 16;
    CHAR tempBuf[TEMP_BUF_SIZE];
    DWORD peekedBytes;
    const auto now = std::chrono::high_resolution_clock::now();

    // Read timeout from conf. If set to 0 use 1 day as timeout.
    const long timeout_config = config::processTimeout.GetValue();
    const auto timeout_s = std::chrono::seconds(timeout_config == 0 ? 86400 : timeout_config);

    // Try to read output from the process. for 10 seconds
    while (!inputFound)
    {
        if (PeekNamedPipe( g_hChildStd_OUT_Rd, tempBuf, TEMP_BUF_SIZE, &peekedBytes, NULL, NULL ))
        {
            if (peekedBytes > 0)
            {
                inputFound = true;
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto td = std::chrono::high_resolution_clock::now() - now;
        if (td > timeout_s) break;
    }

    bool rSuccess = false;
    if (inputFound)
    {
        DWORD dwRead;
        // Should be good enough amount of characters
        const int BUFSIZE = 2048;
        CHAR chBuf[BUFSIZE];
        rSuccess = ReadFile( g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL );
        artwork_url = pfc::string8(chBuf, dwRead);
        // rtrim space like characters
        artwork_url.skip_trailing_chars(" \t\n\r");
    }

    return rSuccess;
}

pfc::string8 getArtworkFilepath(const artwork_info& art, abort_callback &abort, pfc::string8 &tempFile, bool &deleteFile)
{
    abort.check();

    pfc::string8 filepath;

    // Tempfile assigned later to make sure the actual cover art is not deleted after uploading
    tempFile = "";

    deleteFile = false;

    // If cover art is not embedded just use that file as the input.
    // No need to copy it to another folder
    if (strlen(art.path) > 0)
    {
        filepath = pfc::string8(art.path);
    }
    else
    {
        auto tempDir = core_api::pathInProfile(DRP_UNDERSCORE_NAME);

        if (!filesystem::g_exists(tempDir.c_str(), abort))
        {
            filesystem::g_create_directory(tempDir.c_str(), abort);
        }

        // Get the image mime type since we cannot deduce it otherwise from embedded artwork
        const auto api = fb2k::imageLoaderLite::tryGet();
        std::string ext = "jpg";
        if (api.is_valid())
        {
            const auto info = api->getInfo(art.data->get_ptr(), art.data->get_size(), abort);
            abort.check();

            const auto mime = std::string(info.mime);
            // Use mime type extension for image/ mime types. Not perfect and will fail for some of the more exotic types like svg
            if (mime.rfind("image/", 0) == 0)
            {
                ext = mime.substr(mime.find("/") + 1);
            }
        }

        // Take the last 10 digits of current time and use that for filename.
        // Should be good enough for this purpose as the file is deleted after the operation or overwritten in the future
        const auto ts = std::to_string(std::time(NULL));
        const auto filename = pfc::string8((ts.substr(std::max(ts.size(), (size_t)10) - 10) + "." + ext).c_str());

        tempDir.add_filename(filename.c_str());
        filepath = tempDir;
        deleteFile = true;

        #ifdef _DEBUG
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": full temp filepath " << filepath;
        #endif

        // UTF-8 might cause problems?
        tempFile = filepath;
        {
            service_ptr_t<file> file_ptr;
            // File gets released after file_ptr has been deleted
            filesystem::g_open_write_new( file_ptr, tempFile, abort );
            file_ptr->write( art.data->get_ptr(), art.data->get_size(), abort );
        }
    }

    if (filepath.has_prefix("file://"))
    {
        // Remove file:// protocol since the program expects a file path instead of a uri
        if (filepath.replace_string("file://", "") > 1)
        {
            // This should never really be reached
            FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Multiple instances of \"file://\" replaced during cover upload";
        }
    }

    return filepath;
}

std::wstring to_wstring(const std::string &str)
{
    // Just check size
    int convertResult = MultiByteToWideChar(
            CP_UTF8,
            0,
            str.c_str(),
            (DWORD)str.length(),
            NULL,
            0);

    if ( convertResult <= 0 )
    {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Failed to convert command to utf-16. Error code " << convertResult;
        throw std::invalid_argument( "Could not convert command to utf-16" );
    }

    std::wstring str_w;
    // https://docs.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
    // The example has + 10 without any comment as to why
    str_w.resize( convertResult + 10 );

    // This one writes the bytes to str_w
    convertResult = MultiByteToWideChar(
        CP_UTF8,
        0,
        str.c_str(),
        (DWORD)str.length(),
        &str_w[0],
        (int)str_w.size() );

    if ( convertResult <= 0 )
    {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Failed to convert command to utf-16. Error code " << convertResult;
        throw std::invalid_argument( "Could not convert command to utf-16" );
    }

    return str_w;
}

/*
    The previous implementation would use stdin, we're keeping it for backwards compatibility with older scripts.
    When placeholder string {filepath} is found, it'll substitute it and pass it as a positional argument, skipping stdin.
    TODO: Add current URL with {cachedurl}, this would allow a script to verify it's still up and skip it OR pass it back (do we want that?)
*/
std::pair<std::wstring, std::wstring> parseCommand(const std::string &commandString, const std::string &filepath = "", const std::string &url = "")
{
    auto quoteIfNeeded = []( const std::string& value ) -> std::string {
        if ( value.empty() || value.find( ' ' ) == std::string::npos || value[0] == '"' )
        {
            return value;
        }

        return "\"" + value + "\"";
    };
    auto substitutePlaceholder = []( std::string& command,
                                     const std::string& placeholder,
                                     const std::string& value,
                                     const bool autoQuote ) {
        size_t placeholderPos = 0;
        while ( ( placeholderPos = command.find( placeholder, placeholderPos ) ) != std::string::npos )
        {
            const size_t placeholderEnd = placeholderPos + placeholder.length();
            const bool isAlreadyQuoted = ( placeholderPos > 0 && placeholderEnd < command.size()
                                           && command[placeholderPos - 1] == '"'
                                           && command[placeholderEnd] == '"' );

            const auto replacement = ( autoQuote && !isAlreadyQuoted )
                                         ? quoteIfNeeded( value )
                                         : value;

            command.replace( placeholderPos, placeholder.length(), replacement );
            placeholderPos += replacement.length();
        }
    };

    std::string processedCommand = commandString;

    substitutePlaceholder( processedCommand, "{filepath}", filepath, true );
    substitutePlaceholder( processedCommand, "{url}", url, false );

    std::wstring cmd_w = to_wstring(processedCommand);
    std::wstring executable;
    std::wstring fullCmdLine = cmd_w;

    // Find the executable part (first token, handling quotes)
    size_t pos = 0;
    if (cmd_w[0] == L'"') {
        // Quoted executable path
        pos = cmd_w.find(L'"', 1);
        if (pos != std::wstring::npos) {
            executable = cmd_w.substr(1, pos - 1); // remove quotes
            pos++; // move past the closing quote
        }
    } else {
        // Unquoted executable path - find first space
        pos = cmd_w.find(L' ');
        if (pos != std::wstring::npos) {
            executable = cmd_w.substr(0, pos);
        } else {
            executable = cmd_w; // no arguments
        }
    }

    return std::make_pair(executable, fullCmdLine);
}

/*
    Check if command uses file path placeholder
*/
bool usesFilePathPlaceholder(const std::string &commandString)
{
    return commandString.find("{filepath}") != std::string::npos;
}

/*
    Validate command string
*/
bool validateCommandString(const std::string &commandString, std::string &errorMessage)
{
    // Check for empty/whitespace-only command
    if (commandString.empty() || commandString.find_first_not_of(" \t\n\r") == std::string::npos) {
        errorMessage = "Invalid command string: is empty or only contains whitespaces.";
        return false;
    }

    // Basic quote matching check
    size_t quoteCount = 0;
    for (char c : commandString) {
        if (c == '"') quoteCount++;
    }
    if (quoteCount % 2 != 0) {
        errorMessage = "Invalid command string: quotes do not match.";
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Warning: " << errorMessage;
        // Keep going, receiver might handle it fine..?
        // Consider adding a popup warning for certain ones?
    }

    return true;
}

/*
    Get Windows error description from error code
*/
std::string getWindowsErrorDescription(DWORD errorCode)
{
    LPSTR messageBuffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);

    if (size && messageBuffer) {
        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);

        // Remove trailing return or newline
        while (!message.empty() && (message.back() == '\r' || message.back() == '\n')) {
            message.pop_back();
        }
        return message;
    }

    return "Unknown error";
}

/*
    Simple validation, does it look like a normal URL? Insert <falsehoods people believe about URLs> here
*/
bool isValidUrl(const pfc::string8 &url)
{
    if (url.get_length() == 0) {
        return false;
    }

    // Check for basic URL patterns
    const char* url_cstr = url.c_str();
    return (strncmp(url_cstr, "http://", 7) == 0 ||
            strncmp(url_cstr, "https://", 8) == 0);
}

bool uploadOpenProcess(const std::wstring &executable, const std::wstring &cmd_w, const char* filepath_c, pfc::string8 &artwork_url,
    STARTUPINFO &siStartInfo, PROCESS_INFORMATION &piProcInfo,
    HANDLE &g_hChildStd_OUT_Wr, HANDLE &g_hChildStd_IN_Rd, HANDLE &g_hChildStd_IN_Wr, HANDLE &g_hChildStd_OUT_Rd, bool useStdinForFilepath)
{
     // Create the child process
     std::wstring mutableCmdLine = cmd_w; // https://devblogs.microsoft.com/oldnewthing/20090601-00/?p=18083
     bool bSuccess = CreateProcessW(
        executable.empty() ? NULL : executable.c_str(), // application name
        (LPWSTR)mutableCmdLine.c_str(), // command line
        NULL,          // process security attributes
        NULL,          // primary thread security attributes
        TRUE,          // handles are inherited
        CREATE_NO_WINDOW, // creation flags
        NULL,          // use parent's environment
        NULL,          // use parent's current directory
        &siStartInfo,  // STARTUPINFO pointer
        &piProcInfo);  // receives PROCESS_INFORMATION

     if (bSuccess)
     {
         // Close handles to the stdin and stdout pipes no longer needed by the child process.
         // If they are not explicitly closed, there is no way to recognize that the child process has ended.
         CloseHandle( g_hChildStd_OUT_Wr );
         g_hChildStd_OUT_Wr = NULL;
         CloseHandle( g_hChildStd_IN_Rd );
         g_hChildStd_IN_Rd = NULL;

         try
         {
             bool wSuccess = true;

             // stdin if we're using no placeholder, old behaviour for backwards compatibility so it doesn't break somebody's script
             if (useStdinForFilepath) {
                 DWORD dwWritten;
                 wSuccess = WriteFile(g_hChildStd_IN_Wr, filepath_c, (DWORD)strlen( filepath_c ), &dwWritten, NULL );
             }

             CloseHandle( g_hChildStd_IN_Wr );
             g_hChildStd_IN_Wr = NULL;

             bool rSuccess = readFromPipe(g_hChildStd_OUT_Rd, artwork_url);

             if ( wSuccess && rSuccess )
             {
                 WaitForSingleObject( piProcInfo.hProcess, 5000 );
             }
         }
         catch (...)
         {
             // Make sure handles are close in case of error
             CloseHandle( piProcInfo.hProcess );
             CloseHandle( piProcInfo.hThread );
             throw;
         }

         DWORD exit_code;
         GetExitCodeProcess( piProcInfo.hProcess, &exit_code );
         bool terminateProcess = exit_code == STILL_ACTIVE && artwork_url.get_length() == 0;

         // In case of a time out terminate the process
         if (terminateProcess)
         {
             TerminateProcess(&piProcInfo.hProcess, exit_code);
             WaitForSingleObject( piProcInfo.hProcess, 5000 );
         }

         CloseHandle( piProcInfo.hProcess );
         CloseHandle( piProcInfo.hThread );

         // If exit code is zero and result contains newlines assume it's an error since urls should not contains those
         // Also does simple validation on the URL
         const bool isError = exit_code != 0 || artwork_url.find_first('\n') != ~0 || !isValidUrl(artwork_url);
         artwork_url =  terminateProcess ? pfc::string8("Process timed out") : artwork_url;

         FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": artwork uploader exited with status: " << exit_code <<
             " and " << ( isError ? "error" : "url" ) << ": " << artwork_url;

         if (isError)
         {
             artwork_url = "";
         }

         return true;
     }

    // What's the actual error? We care.
    DWORD lastError = GetLastError();
    std::string errorDesc = getWindowsErrorDescription(lastError);

    // Let's get the executable string
    std::string executableStr;
    if (!executable.empty()) {
        int size = WideCharToMultiByte(CP_UTF8, 0, executable.c_str(), -1, NULL, 0, NULL, NULL);
        if (size > 0) {
            executableStr.resize(size - 1); // -1 to exclude null terminator
            WideCharToMultiByte(CP_UTF8, 0, executable.c_str(), -1, &executableStr[0], size, NULL, NULL);
        }
    }

    if (!executableStr.empty()) {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Failed to execute '" << executableStr << "' (Windows error: " << lastError << " - " << errorDesc << ")";
    } else {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Failed to execute command (Windows error: " << lastError << " - " << errorDesc << ")";
    }

    return false;
}

bool usesUrlPlaceholder(const std::string &commandString)
{
    return commandString.find("{url}") != std::string::npos;
}

pfc::string8 uploadArtwork(artwork_info& art, abort_callback &abort, metadb_index_hash hash)
{
    pfc::string8 artwork_url = "";
    pfc::string8 cached_url = "";
    const std::string commandString = config::uploadArtworkCommand.GetValue();
    const bool hasFilePathPlaceholder = usesFilePathPlaceholder(commandString);
    const bool hasUrlPlaceholder = usesUrlPlaceholder(commandString);

    //  Hit both URL stores in priority order
    //  URL check: metadb (uploader but can also be set by user manually), does it exist, and do we think it's valid?
    if (hash != metadb_index_hash())
    {
        auto rec = record_get(hash);
        if (rec.artwork_url.get_length() > 0 && isValidUrl(rec.artwork_url))
        {
            cached_url = rec.artwork_url;
        }
    }
    // URL check: hash list (only the uploader writes to it), lower priority
    if (cached_url.get_length() == 0 && hasArtworkData(art))
    {
        pfc::string8 hash_url;
        check_artwork_hash(art, abort, hash_url);
        if (hash_url.get_length() > 0 && isValidUrl(hash_url))
        {
            cached_url = hash_url;
        }
    }

    // If we have a cached URL but command doesn't use {url} placeholder, return the cached URL
    if ( cached_url.get_length() > 0 )
    {
        if ( commandString.length() == 0 || !hasUrlPlaceholder )
        {
            #ifdef _DEBUG
            FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Using cached URL directly (no {url} placeholder): " << cached_url.c_str();
            #endif
            return cached_url;
        }
        // If command uses {url} placeholder, continue execution w/ cached URL
        #ifdef _DEBUG
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Found cached URL, will call external tool with {url} placeholder: " << cached_url.c_str();
        #endif
    }

    if ( commandString.length() == 0 )
    {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": No upload command given";
        return artwork_url;
    }

    // Validate command string
    std::string validationError;
    if (!validateCommandString(commandString, validationError)) {
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Invalid upload command: " << validationError;
        return artwork_url;
    }

    pfc::string8 tempFile;
    bool deleteFile = false;
    pfc::string8 filepath;

    if (hasArtworkSource(art))
    {
        filepath = getArtworkFilepath(art, abort, tempFile, deleteFile);
        const auto filepath_c = filepath.c_str();

        // Must validate the file path we're substituting the token with exists
        if (hasFilePathPlaceholder && !filesystem::g_exists(filepath_c, abort)) {
            FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Warning: artwork file path does not exist: " << filepath;
            // Keep going, could not be there yet? I think it may trigger and still work *clueless*
        }
    }
    else
    {
        if (!hasUrlPlaceholder)
        {
            FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": No artwork available for upload command";
            return artwork_url;
        }

        if (cached_url.get_length() == 0)
        {
            FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Cannot execute {url} upload command without a cached artwork URL";
            return artwork_url;
        }

        if (hasFilePathPlaceholder)
        {
            FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Cannot execute upload command with {filepath} when artwork extraction failed";
            return cached_url;
        }
    }

    abort.check();

    const auto filepath_c = filepath.c_str();
   
    HANDLE g_hChildStd_IN_Rd = NULL;
    HANDLE g_hChildStd_IN_Wr = NULL;
    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;
    
    SECURITY_ATTRIBUTES saAttr;

     try
     {
         if ( !initializeProcessVariables(g_hChildStd_IN_Rd, g_hChildStd_IN_Wr, g_hChildStd_OUT_Rd, g_hChildStd_OUT_Wr, saAttr) )
         {
             closePipes(g_hChildStd_IN_Rd, g_hChildStd_IN_Wr, g_hChildStd_OUT_Rd, g_hChildStd_OUT_Wr);
             return artwork_url;
         }

         STARTUPINFO siStartInfo;

         ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
         siStartInfo.cb = sizeof(STARTUPINFO);
         siStartInfo.hStdError = g_hChildStd_OUT_Wr;
         siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
         siStartInfo.hStdInput = g_hChildStd_IN_Rd;
         siStartInfo.dwFlags |= STARTF_USESTDHANDLES;


         PROCESS_INFORMATION piProcInfo; 
         ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

         #ifdef _DEBUG
         FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Upload command " << commandString;
         FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Cover file path " << filepath;
         if (hasUrlPlaceholder) {
             FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Cached URL being passed: " << cached_url.c_str();
         }
         #endif

         // Parse the command to separate executable from full command line
         // Pass filepath and/or URL if placeholders are used
         const auto parsedCmd = parseCommand(commandString,
             hasFilePathPlaceholder ? filepath.c_str() : "",
             hasUrlPlaceholder ? cached_url.c_str() : "");
         const auto& executable = parsedCmd.first;
         const auto& fullCmdLine = parsedCmd.second;

         #ifdef _DEBUG
         qwr::u8string fullCmd_utf8;
         fullCmd_utf8 = qwr::unicode::ToU8(fullCmdLine);
         FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Final command after placeholder substitution: " << fullCmd_utf8;
         #endif

         abort.check();

         // If using any placeholder, skip writing to stdin, the user should not need that
         // If not using *any* placeholder, use stdin for backwards compatibility, assume user has older upload script
         uploadOpenProcess(executable, fullCmdLine, filepath_c, artwork_url,
             siStartInfo, piProcInfo,
             g_hChildStd_OUT_Wr, g_hChildStd_IN_Rd, g_hChildStd_IN_Wr, g_hChildStd_OUT_Rd, !(hasFilePathPlaceholder || hasUrlPlaceholder));

         #ifdef _DEBUG
         FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": External command returned: '" << (artwork_url.get_length() > 0 ? artwork_url.c_str() : "(empty)") << "'";
         #endif

         // If using {url} placeholder and external command returned empty, return cached URL directly
         if (hasUrlPlaceholder && artwork_url.get_length() == 0) {
             #ifdef _DEBUG
             FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": External command returned empty, falling back to cached URL: " << cached_url.c_str();
             #endif
             if ( deleteFile )
             {
                 filesystem::g_remove(tempFile, abort);
             }
             return cached_url;
         }
     } catch (...)
     {
         closePipes(g_hChildStd_IN_Rd, g_hChildStd_IN_Wr, g_hChildStd_OUT_Rd, g_hChildStd_OUT_Wr);

         if ( deleteFile )
         {
             filesystem::g_remove( tempFile, abort );
         }
         
         FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": artwork uploader threw an error";
         throw;
     }

    if ( deleteFile )
    {
        filesystem::g_remove(tempFile, abort);
    }

    abort.check();

    if (artwork_url.get_length() > 0)
    {
        ensureArtworkHash(art);
        if (art.artwork_hash.get_length() > 0)
        {
            set_artwork_url_hash(artwork_url, art.artwork_hash, abort);
        }
        #ifdef _DEBUG
        FB2K_console_formatter() << DRP_NAME_WITH_VERSION << ": Using external command result: " << artwork_url.c_str();
        #endif
    }

    return artwork_url;
}

}
