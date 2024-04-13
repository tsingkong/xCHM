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

#ifndef __CHMINPUTSTREAM_H_
#define __CHMINPUTSTREAM_H_

#include <chmfile.h>
#include <memory>
#include <wx/stream.h>

/*!
  \class wxInputStream
  \brief wxWidgets input stream class.
*/

//! Input stream from a .chm archive.
class CHMInputStream : public wxInputStream {
public:
    /*!
      \brief Creates a stream.
      \param archive The .chm file name on disk. It makes sense to
      pass the empty string if you're sure file is a link to a
      page, in the form of "/MS-ITS:archive.chm::/filename.html".
      \param file The file requested from the archive.
     */
    CHMInputStream(const wxString& archive, const wxString& file);

    //! Returns the size of the file.
    size_t GetSize() const override;

    //! True if EOF has been found.
    bool Eof() const override;

    /*!
      \brief Returns the static CHMFile pointer associated with
      this stream. Archives are being cached until it is
      explicitly requested to open a different one.
      \return A valid pointer to a CHMFile object or nullptr if no
      .chm file has been opened yet.
     */
    static CHMFile* GetCache();

    /*!
      \brief Cleans up the cache. Has to be public and static
      since the stream doesn't know how many other streams using
      the same cache will be created after it. Somebody else has
      to turn off the lights, and in this case it's CHMFSHandler.
     */
    static void Cleanup();

protected:
    /*!
      \brief Attempts to read a chunk from the stream.
      \param buffer The read data is being placed here.
      \param bufsize Number of bytes requested.
      \return Number of bytes actually read.
    */
    size_t OnSysRead(void* buffer, size_t bufsize) override;

    /*!
      \brief Seeks to the requested position in the file.
      \param seek Where to seek.
      \param mode Seek from the beginning, current position, etc.
      \return Position in the file.
     */
    wxFileOffset OnSysSeek(wxFileOffset seek, wxSeekMode mode) override;

    /*!
      \brief Asks what is the current position in the file.
      \return Current position.
     */
    wxFileOffset OnSysTell() const override { return _currPos; }

private:
    //! Helper. Inits the cache.
    bool Init(const wxString& archive);

private:
    bool _override_object;
    std::string _object_new;

private:
    static std::unique_ptr<CHMFile> _archiveCache;
    off_t                           _currPos {0};
    chmUnitInfo                     _ui {};
    static wxString                 _path;
};

#endif // __CHMINPUTSTREAM_H_
