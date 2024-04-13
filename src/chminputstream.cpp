/*
  Copyright (C) 2003 - 2024  Razvan Cojocaru <rzvncj@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
  MA 02110-1301, USA.
*/

#include <chminputstream.h>
// #include <chardet.h>

/*----------------------------------------------------------------------
 * class CHMInputStream static members
 */

std::unique_ptr<CHMFile> CHMInputStream::_archiveCache;
wxString                 CHMInputStream::_path;

void CHMInputStream::Cleanup()
{
    _archiveCache.reset();
}

CHMFile* CHMInputStream::GetCache()
{
    return _archiveCache.get();
}

std::string& replace_substr(std::string& input, const std::string& old, const std::string& replace_by)
{
    // Find the first occurrence of the substring
    size_t pos = input.find(old);

    // Iterate through the string and replace all
    // occurrences
    while (pos != std::string::npos) {
        // Replace the substring with the specified string
        input.replace(pos, old.size(), replace_by);

        // Find the next occurrence of the substring
        pos = input.find(old, pos + replace_by.size());
    }
    return input;
}

/*----------------------------------------------------------------------
 * rest of class CHMInputStream implementation
 */

CHMInputStream::CHMInputStream(const wxString& archive, const wxString& file)
{
    extern wxString g_charset;
    auto filename = file;

    if (!archive.IsEmpty())
        _path = archive.BeforeLast(wxT('/')) + wxT("/");

    // Maybe the cached chmFile* isn't valid anymore, or maybe there is no chached chmFile* yet.
    if (!archive.IsEmpty() && !Init(archive)) {
        m_lasterror = wxSTREAM_READ_ERROR;
        return;
    }

    assert(_archiveCache);

    // Somebody's looking for the homepage.
    if (file.IsSameAs(wxT("/")))
        filename = _archiveCache->HomePage();

    if (!filename.Left(8).CmpNoCase(wxT("/MS-ITS:"))) {
        // If this ever happens chances are Microsoft decided that even if we went through the
        // trouble to open this archive and check out the index file, the index file is just a
        // link to a file in another archive.

        auto arch_link = filename.AfterFirst(wxT(':')).BeforeFirst(wxT(':'));

        filename = filename.AfterLast(wxT(':'));

        // Reset the cached chmFile* and all.
        if (!Init(arch_link))
            if (!Init(_path + arch_link)) {
                m_lasterror = wxSTREAM_READ_ERROR;
                return;
            }
    }

    // See if the file really is in the archive.
    if (!_archiveCache->ResolveObject(filename, &_ui)) {
        printf("%s[%d]wxSTREAM_READ_ERROR:%s\n", __func__, __LINE__, filename.ToStdString().c_str());
        m_lasterror = wxSTREAM_READ_ERROR;
        return;
    }

#ifdef _DEBUG
    printf("%s[%d]_ui.path:%s\n", __func__, __LINE__, _ui.path);
#endif

    bool is_txt = filename.Right(4).IsSameAs(".txt", false);
    bool is_html = false;
    bool is_html_gb2312 = false;
    size_t i_gb2312_position = std::string::npos;
    bool is_html_missing_content_type = false;  // leak "http-equiv=\"content-type\"", as <meta charset=xxx> is not support
    size_t i_tag_head = std::string::npos;      // position of "<head>"
    bool  is_no_space_before_charset = false;   // ";charset=" => "; charset="
    size_t i_charset = std::string::npos;
    {
        char buffer[512] = {0};
        int bufsize = 500;
        bufsize = _archiveCache->RetrieveObject(&_ui, reinterpret_cast<unsigned char*>(buffer), 0, bufsize);
        // to_lower
        for (int i = 0; i < bufsize; i++) {
            buffer[i] = tolower(buffer[i]);
        }
        if (filename.Right(4).IsSameAs(".htm", false) || filename.Right(5).IsSameAs(".html", false) || 
                (strstr(buffer, "<html>") != NULL)) {
            is_html = true;
            char* p_gb2312 = strstr(buffer, "gb2312");
            if (p_gb2312 != NULL) {
                is_html_gb2312 = true;
                i_gb2312_position = p_gb2312 - buffer;
            }
            if (strstr(buffer, "http-equiv=\"content-type\"") == NULL) {
                is_html_missing_content_type = true;
            }
            if (strstr(buffer, ";charset=") != NULL) {
                is_no_space_before_charset = true;
            }
            char* p_head = strstr(buffer, "<head>");
            if (p_head != NULL) {
                i_tag_head = p_head - buffer;
            }
            char* p_charset = strstr(buffer, "charset=");
            if (p_charset != NULL) {
                i_charset = p_charset - buffer;
            }
#ifdef _DEBUG
            printf("%s[%d]raw file:%s\n", __func__, __LINE__, buffer);
#endif
        }
    }

    _override_object = false;
    _object_new.clear();
    do {
        if (!is_txt && !is_html_gb2312 && !is_html_missing_content_type && !is_no_space_before_charset) {
            break;
        }

        // read raw content
        LONGUINT64 read_done = 0;
        std::string _object_raw;
        while(read_done < _ui.length) {
            char buffer[1024] = {0};
            int bufsize = 1023;
            bufsize = _archiveCache->RetrieveObject(&_ui, reinterpret_cast<unsigned char*>(buffer), read_done, bufsize);
            _object_raw += buffer;
            read_done += bufsize;
        }

        if (is_txt) {
            // detect
            /*
            int error = 0;
            Detect    * det;
            DetectObj * det_obj;
            if ( (det = detect_init ()) == NULL ) {
                fprintf (stderr, "chardet handle initialize failed\n");
                error = CHARDET_MEM_ALLOCATED_FAIL;
                break;
            }
            detect_reset (&det);
            if ( (det_obj = detect_obj_init ()) == NULL ) {
                fprintf (stderr, "Memory Allocation failed\n");
                error = CHARDET_MEM_ALLOCATED_FAIL;
                break;
            }
            error = detect_handledata_r (&det, _object_raw.c_str(), _object_raw.length(), &det_obj);
            if (error == CHARDET_OUT_OF_MEMORY) {
                fprintf (stderr, "On handle processing, occured out of memory\n");
                detect_obj_free (&det_obj);
                break;
            }
            if (error == CHARDET_NULL_OBJECT) {
                fprintf (stderr, "2st argument of chardet() is must memory allocation with detect_obj_init API\n");
                break;
            }

            printf ("%s[%d]encoding: %s, confidence: %f, exist BOM: %d\n", __func__, __LINE__, det_obj->encoding, det_obj->confidence, det_obj->bom);
            std::string _charset = det_obj->encoding;
            detect_obj_free (&det_obj);
            detect_destroy (&det);
            */

            //wxCSConv conv(_charset);
            //printf("%s[%d]wxCSConv(%s):%s\n", __func__, __LINE__, _charset.c_str(), conv.IsOk() ? "OK" : "Fail");
            //wxString new_body(_object_raw.c_str(), conv);
            //_object_new = new_body.ToStdString();
            //printf ("old_object:%s\n", _object_raw.substr(0, 100).c_str());

            std::string head_1 = R"(
                <html>
                <head>
                <meta http-equiv="Content-Type" content="text/html; charset=)";
            std::string head_2 = R"("/>
                <title>title</title>
                </head>
                <body>)";
            std::string tail = R"(
                </body>
                </html>)";

            replace_substr(_object_raw, " ", "&nbsp;");
            replace_substr(_object_raw, "\r\n", "<br />");
            replace_substr(_object_raw, "\n\r", "<br />");
            replace_substr(_object_raw, "\r", "<br />");
            replace_substr(_object_raw, "\n", "<br />");

            // _object_new = head_1 + g_charset + head_2 + _object_raw + tail;
            // 上面的写法失效，原因待查 TODO
            _object_new = head_1 + g_charset + head_2;
            _object_new = _object_new + _object_raw;
            _object_new = _object_new + tail;
            _override_object = true;
        }
        else if (is_html_gb2312 && i_gb2312_position != std::string::npos) {
            printf("%s[%d]replace gb2312 at %ld with GB18030\n", __func__, __LINE__, i_gb2312_position);
            _object_new = _object_raw;
            _object_new.replace(i_gb2312_position, 6, "GB18030");
            _override_object = true;
        }
        else if (is_html_missing_content_type && i_tag_head != std::string::npos) {
            i_tag_head += strlen("<head>");
            printf("%s[%d]insert content_type at %ld\n", __func__, __LINE__, i_tag_head);
            std::string s_content_type = "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=" + g_charset.ToStdString() + "\"/>";
            _object_new = _object_raw;
            _object_new.insert(i_tag_head, s_content_type);
            _override_object = true;
        }
        else if (is_no_space_before_charset && i_charset != std::string::npos) {
            printf("%s[%d]insert space at %ld\n", __func__, __LINE__, i_charset);
            _object_new = _object_raw;
            _object_new.insert(i_charset, " ");
            _override_object = true;
        }

        if (!_override_object) {
            break;
        }
        for (int i = 0; i < 48; i++) {
            // printf("%02X ", (unsigned char)_object_new[i]);
        }
#ifdef _DEBUG
        printf ("%s[%d]new_object:%s\n", __func__, __LINE__, _object_new.substr(0, 500).c_str());
#endif
    }while(false);
}

size_t CHMInputStream::GetSize() const
{
    if (_override_object) {
        size_t size = _object_new.length();
#ifdef _DEBUG
        printf("%s[%d]size:%ld\n", __func__, __LINE__, size);
#endif
        return size;
    }
    return _ui.length;
}

bool CHMInputStream::Eof() const
{
    if (_override_object) {
        return static_cast<uint64_t>(_currPos) >= GetSize();
    }
    return static_cast<uint64_t>(_currPos) >= _ui.length;
}

size_t CHMInputStream::OnSysRead(void* buffer, size_t bufsize)
{
    if (_override_object) {
#ifdef _DEBUG
        printf("%s[%d]_currPos=%ld, bufsize=%ld\n", __func__, __LINE__, _currPos, bufsize);
#endif
        if (static_cast<uint64_t>(_currPos) >= GetSize()) {
            m_lasterror = wxSTREAM_EOF;
            return 0;
        }

        if (!_archiveCache)
            return 0;

        if (static_cast<uint64_t>(_currPos + bufsize) > GetSize())
            bufsize = GetSize() - _currPos;

        memcpy(buffer, _object_new.c_str() + _currPos, bufsize);
        _currPos += bufsize;
        return bufsize;
    }

    if (static_cast<uint64_t>(_currPos) >= _ui.length) {
        m_lasterror = wxSTREAM_EOF;
        return 0;
    }

    if (!_archiveCache)
        return 0;

    if (static_cast<uint64_t>(_currPos + bufsize) > _ui.length)
        bufsize = _ui.length - _currPos;

    bufsize = _archiveCache->RetrieveObject(&_ui, static_cast<unsigned char*>(buffer), _currPos, bufsize);
    _currPos += bufsize;

    return bufsize;
}

wxFileOffset CHMInputStream::OnSysSeek(wxFileOffset seek, wxSeekMode mode)
{
    switch (mode) {
    case wxFromCurrent:
        _currPos += seek;
        break;
    case wxFromStart:
        _currPos = seek;
        break;
    case wxFromEnd:
        if (_override_object) {
            _currPos = GetSize() - 1 + seek;
            break;
        }
        _currPos = _ui.length - 1 + seek;
        break;
    default:
        _currPos = seek;
    }

    return _currPos;
}

bool CHMInputStream::Init(const wxString& archive)
{
    if (!_archiveCache || !_archiveCache->ArchiveName().IsSameAs(archive)) {
        _archiveCache = std::make_unique<CHMFile>(archive);

        if (!_archiveCache->IsOk()) {
            Cleanup();
            return false;
        }
    }

    return true;
}
