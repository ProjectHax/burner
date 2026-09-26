Name:           burner
Version:        0.1.0
Release:        1%{?dist}
Summary:        CD/DVD/Blu-ray Burning Application

License:        GPL-3.0-or-later
URL:            https://github.com/projecthax/burner
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.21
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.2
# qt6-linguist or qt6-qttools-devel for lrelease (optional - translations pre-compiled)
BuildRequires:  libburn-devel
BuildRequires:  libisofs-devel
BuildRequires:  libisoburn-devel
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(openssl)
# FFmpeg - use pkgconfig to work with both ffmpeg-devel and ffmpeg-free-devel
BuildRequires:  pkgconfig(libavcodec)
BuildRequires:  pkgconfig(libavformat)
BuildRequires:  pkgconfig(libavutil)
BuildRequires:  pkgconfig(libswresample)
BuildRequires:  pkgconfig(libswscale)
BuildRequires:  desktop-file-utils

Requires:       qt6-qtbase
Requires:       qt6-qtwayland
Requires:       libburn
Requires:       libisofs
Requires:       libisoburn
Requires:       libcurl
Requires:       openssl-libs

%description
Burner is a modern CD/DVD/Blu-ray burning application built with Qt6.
It supports burning data discs, audio CDs, ISO images, video discs,
disc cloning operations, and audio CD ripping with MusicBrainz metadata lookup.

Features:
- Data disc burning with ISO9660, Joliet, and Rock Ridge support
- Audio CD burning with automatic format conversion
- ISO image burning and creation
- Video disc creation (VCD, SVCD, DVD-Video)
- Disc cloning and copying
- Audio CD ripping to FLAC, MP3, AAC, OGG, or WAV
- MusicBrainz metadata and cover art lookup
- Multi-language support

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

# Install desktop file
install -Dm644 packaging/burner.desktop %{buildroot}%{_datadir}/applications/burner.desktop
desktop-file-validate %{buildroot}%{_datadir}/applications/burner.desktop

# Install icon
install -Dm644 packaging/burner.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/burner.svg

# Install translations
install -d %{buildroot}%{_datadir}/burner/translations
install -m644 translations/*.qm %{buildroot}%{_datadir}/burner/translations/ 2>/dev/null || true

%files
%license LICENSE
%doc README.md
%{_bindir}/burner
%{_datadir}/applications/burner.desktop
%{_datadir}/icons/hicolor/scalable/apps/burner.svg
%{_datadir}/burner/

# build-rpm.sh replaces the changelog with an entry for the version being built
%changelog
* Sun Sep 27 2026 ProjectHax LLC - 0.1.0-1
- Initial release
