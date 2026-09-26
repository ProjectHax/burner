#!/usr/bin/env python3
"""
Polish translation script for burner_pl.ts
Translates all unfinished entries from English to Polish
"""

import os
import re
import sys

# Translation dictionary for common terms and phrases
TRANSLATIONS = {
    # Files and paths
    "File not found: %1": "Nie znaleziono pliku: %1",
    "Directory not readable: %1": "Katalog nie do odczytania: %1",
    "File not readable: %1": "Plik nie do odczytania: %1",
    "Cannot open file: %1\nReason: %2": "Nie można otworzyć pliku: %1\nPowód: %2",
    "Cannot read file: %1\nReason: %2": "Nie można odczytać pliku: %1\nPowód: %2",
    "File validation failed:\n\n%1": "Sprawdzanie pliku nie powiodło się:\n\n%1",

    # Validation
    "Validating files...": "Sprawdzanie plików...",
    "File validation failed": "Sprawdzanie plików nie powiodło się",
    "Validating audio files...": "Sprawdzanie plików dźwiękowych...",
    "Audio file validation failed": "Sprawdzanie plików dźwiękowych nie powiodło się",
    "Validating video files...": "Sprawdzanie plików wideo...",
    "Video file validation failed": "Sprawdzanie plików wideo nie powiodło się",
    "Validating ISO file...": "Sprawdzanie pliku ISO...",
    "ISO file validation failed": "Sprawdzanie pliku ISO nie powiodło się",
    "Validating image file...": "Sprawdzanie pliku obrazu...",
    "Image file validation failed": "Sprawdzanie pliku obrazu nie powiodło się",

    # Initialization
    "Initializing burn engine...": "Inicjalizacja silnika nagrywania...",
    "Failed to initialize burn engine for device: %1": "Nie udało się zainicjalizować silnika nagrywania dla urządzenia: %1",
    "Engine initialization failed": "Inicjalizacja silnika nie powiodła się",
    "Initializing...": "Inicjalizacja...",
    "Failed to initialize engine for device: %1": "Nie udało się zainicjalizować silnika dla urządzenia: %1",
    "Initializing disc cloner...": "Inicjalizacja klonowania dysku...",
    "Failed to initialize cloner for device: %1": "Nie udało się zainicjalizować klonowania dla urządzenia: %1",
    "Cloner initialization failed": "Inicjalizacja klonowania nie powiodła się",
    "Initializing Video CD builder...": "Inicjalizacja buildera Video CD...",
    "Failed to initialize burn engine": "Nie udało się zainicjalizować silnika nagrywania",

    # Building/Burning
    "Building disc image...": "Tworzenie obrazu dysku...",
    "Building ISO image...": "Tworzenie obrazu ISO...",
    "Burning ISO file...": "Nagrywanie pliku ISO...",
    "Preparing audio CD with %1 tracks...": "Przygotowywanie Audio CD z %1 utworami...",
    "Writing ISO file (%1 bytes)...": "Zapisywanie pliku ISO (%1 bajtów)...",
    "Burning Video CD...": "Nagrywanie Video CD...",

    # Progress states
    "Cancelled": "Anulowano",
    "Operation cancelled by user": "Operacja anulowana przez użytkownika",
    "Operation cancelled": "Operacja anulowana",
    "Idle": "Bezczynny",
    "Preparing": "Przygotowywanie",
    "Blanking disc": "Czyszczenie dysku",
    "Writing": "Zapisywanie",
    "Closing disc": "Zamykanie dysku",
    "Verifying": "Weryfikacja",
    "Completed": "Ukończono",
    "Failed": "Niepowodzenie",
    "Ready": "Gotowy",
    "Initialization failed": "Inicjalizacja nie powiodła się",
    "Cancelling...": "Anulowanie...",

    # Success messages
    "Burn completed successfully": "Nagrywanie zakończone pomyślnie",
    "Audio CD burn completed successfully": "Nagrywanie Audio CD zakończone pomyślnie",
    "Blank completed successfully": "Czyszczenie zakończone pomyślnie",
    "Disc image created successfully": "Obraz dysku utworzony pomyślnie",
    "Image written to disc successfully": "Obraz zapisany na dysk pomyślnie",
    "Video CD burn completed successfully": "Nagrywanie Video CD zakończone pomyślnie",
    "ISO file created successfully: %1": "Plik ISO utworzony pomyślnie: %1",
    "Successfully ripped %1 track(s)": "Pomyślnie zgranych %1 utworów",

    # Error messages
    "No files to burn": "Brak plików do nagrania",
    "No files added to disc": "Brak plików dodanych do dysku",
    "No audio files to burn": "Brak plików dźwiękowych do nagrania",
    "No audio files provided": "Nie podano plików dźwiękowych",
    "No video files to burn": "Brak plików wideo do nagrania",
    "No video files provided": "Nie podano plików wideo",
    "No files to add to ISO": "Brak plików do dodania do ISO",
    "No files provided": "Nie podano plików",
    "No files added": "Nie dodano plików",
    "No videos to burn": "Brak filmów do nagrania",
    "No videos added": "Nie dodano filmów",
    "No encoded files available": "Brak zakodowanych plików",
    "Encoding produced no output": "Kodowanie nie wygenerowało wyniku",

    # Operations
    "Starting burn with %1 files (%2 bytes)...": "Rozpoczynanie nagrywania z %1 plikami (%2 bajtów)...",
    "Performing full blank...": "Wykonywanie pełnego czyszczenia...",
    "Performing quick blank...": "Wykonywanie szybkiego czyszczenia...",
    "Reading disc to image...": "Odczytywanie dysku do obrazu...",
    "Writing image to disc...": "Zapisywanie obrazu na dysk...",
    "Adding videos...": "Dodawanie filmów...",
    "Failed to add video: %1": "Nie udało się dodać filmu: %1",
    "Failed to add video": "Nie udało się dodać filmu",
    "Adding: %1": "Dodawanie: %1",
    "Writing ISO: %1 / %2 MB": "Zapisywanie ISO: %1 / %2 MB",

    # Errors
    "A burn operation is already in progress": "Operacja nagrywania już trwa",
    "An operation is already in progress": "Operacja już trwa",
    "Engine not initialized: %1": "Silnik nie zainicjalizowany: %1",
    "No output path specified": "Nie określono ścieżki wyjściowej",
    "No image file specified": "Nie określono pliku obrazu",
    "Failed to write ISO file: %1": "Nie udało się zapisać pliku ISO: %1",
    "ISO file creation failed": "Utworzenie pliku ISO nie powiodło się",
    "Total video duration exceeds disc capacity": "Całkowity czas trwania filmu przekracza pojemność dysku",
    "Videos too long for disc": "Filmy za długie dla dysku",
    "Failed to create temporary directory": "Nie udało się utworzyć katalogu tymczasowego",
    "Temporary directory creation failed": "Utworzenie katalogu tymczasowego nie powiodło się",
    "Encoding videos to %1 format...": "Kodowanie filmów do formatu %1...",

    # Drive operations
    "Ejected %1": "Wysunięto %1",
    "Failed to eject %1": "Nie udało się wysunąć %1",
    "Failed to access drive: %1": "Nie udało się uzyskać dostępu do napędu: %1",
    "No audio CD detected in drive": "Nie wykryto Audio CD w napędzie",
    "Failed to read disc: %1": "Nie udało się odczytać dysku: %1",
    "Failed to access drive": "Nie udało się uzyskać dostępu do napędu",

    # Ripping
    "Scanning disc...": "Skanowanie dysku...",
    "Looking up metadata...": "Wyszukiwanie metadanych...",
    "Fetching cover art...": "Pobieranie okładki...",
    "Starting rip...": "Rozpoczynanie zgrywania...",

    # Table headers
    "#": "#",
    "Title": "Tytuł",
    "Artist": "Artysta",
    "Duration": "Czas trwania",
    "Name": "Nazwa",
    "Size": "Rozmiar",
    "Local Path": "Ścieżka lokalna",
    "Track %1": "Utwór %1",

    # UI Elements
    "Audio CD": "Audio CD",
    "Red Book CD-DA (44.1kHz 16-bit Stereo)": "Red Book CD-DA (44.1kHz 16-bit Stereo)",
    "Total: 0:00 / 80:00": "Całkowity: 0:00 / 80:00",
    "Remove": "Usuń",
    "Drop audio files here\n(MP3, FLAC, WAV, OGG)": "Upuść pliki dźwiękowe tutaj\n(MP3, FLAC, WAV, OGG)",
    "Add Tracks...": "Dodaj utwory...",
    "Move Up": "Przesuń w górę",
    "Move Down": "Przesuń w dół",
    "Clear All": "Wyczyść wszystko",
    "Add Files...": "Dodaj pliki...",
    "Add Folder...": "Dodaj folder...",
    "Create Folder...": "Utwórz folder...",
    "Rename...": "Zmień nazwę...",
    "Delete": "Usuń",
    "Select All": "Zaznacz wszystko",
    "Deselect All": "Odznacz wszystko",
    "Expand All": "Rozwiń wszystko",
    "Collapse All": "Zwiń wszystko",

    # Dialogs and pages
    "Burn": "Nagraj",
    "Cancel": "Anuluj",
    "Close": "Zamknij",
    "OK": "OK",
    "Yes": "Tak",
    "No": "Nie",
    "Browse...": "Przeglądaj...",
    "Select output directory": "Wybierz katalog wyjściowy",
    "Select ISO file": "Wybierz plik ISO",
    "Select image file": "Wybierz plik obrazu",
    "Select audio files": "Wybierz pliki dźwiękowe",
    "Select video files": "Wybierz pliki wideo",

    # Settings
    "Burn Speed": "Prędkość nagrywania",
    "Maximum": "Maksymalna",
    "Verify after burn": "Weryfikuj po nagraniu",
    "Eject disc when finished": "Wysuń dysk po zakończeniu",
    "Finalize disc (close session)": "Finalizuj dysk (zamknij sesję)",
    "Simulation mode (test without burning)": "Tryb symulacji (test bez nagrywania)",
    "Multisession": "Wielosesyjny",
    "Allow adding more data later": "Pozwól na późniejsze dodanie danych",
    "Buffer Underrun Protection": "Ochrona przed opróżnieniem bufora",
    "Prevent buffer underruns": "Zapobiega opróżnieniu bufora",
    "Pad tracks to fit Red Book standard": "Wypełnij utwory zgodnie ze standardem Red Book",
    "Write mode": "Tryb zapisu",
    "Track-at-Once (TAO)": "Track-at-Once (TAO)",
    "Session-at-Once (SAO)": "Session-at-Once (SAO)",
    "Disc-at-Once (DAO)": "Disc-at-Once (DAO)",

    # Disc types
    "Video CD Format": "Format Video CD",
    "VCD 1.1 (MPEG-1, 352x240, 1150 kbps)": "VCD 1.1 (MPEG-1, 352x240, 1150 kbps)",
    "VCD 2.0 (MPEG-1, 352x240, 1150 kbps)": "VCD 2.0 (MPEG-1, 352x240, 1150 kbps)",
    "SVCD (MPEG-2, 480x480, 2500 kbps)": "SVCD (MPEG-2, 480x480, 2500 kbps)",

    # File systems
    "File System": "System plików",
    "ISO 9660": "ISO 9660",
    "ISO 9660 + Joliet": "ISO 9660 + Joliet",
    "ISO 9660 + Rock Ridge": "ISO 9660 + Rock Ridge",
    "ISO 9660 + Joliet + Rock Ridge": "ISO 9660 + Joliet + Rock Ridge",
    "UDF": "UDF",
    "ISO 9660 + UDF": "ISO 9660 + UDF",

    # Volume settings
    "Volume Label": "Etykieta woluminu",
    "Volume label (32 characters max)": "Etykieta woluminu (maks. 32 znaki)",
    "Publisher": "Wydawca",
    "Publisher name": "Nazwa wydawcy",
    "Preparer": "Przygotowujący",
    "Preparer name": "Nazwa przygotowującego",
    "Application": "Aplikacja",
    "Application ID": "ID aplikacji",

    # Progress
    "Encoding: Track %1 of %2": "Kodowanie: Utwór %1 z %2",
    "Ripping: Track %1 of %2": "Zgrywanie: Utwór %1 z %2",
    "Track %1: %2": "Utwór %1: %2",
    "Elapsed: %1": "Upłynęło: %1",
    "Remaining: %1": "Pozostało: %1",
    "Speed: %1x": "Prędkość: %1x",
    "Buffer: %1%": "Bufor: %1%",

    # Status
    "Total size: %1": "Całkowity rozmiar: %1",
    "Total duration: %1": "Całkowity czas: %1",
    "Files: %1": "Pliki: %1",
    "Folders: %1": "Foldery: %1",
    "Tracks: %1": "Utwory: %1",

    # Errors and warnings
    "Error": "Błąd",
    "Warning": "Ostrzeżenie",
    "Information": "Informacja",
    "Confirm": "Potwierdź",
    "Are you sure?": "Czy jesteś pewien?",
    "This operation cannot be undone.": "Ta operacja nie może być cofnięta.",
    "The disc is not empty. Do you want to blank it first?": "Dysk nie jest pusty. Czy chcesz go najpierw wyczyścić?",
    "The disc is not empty.": "Dysk nie jest pusty.",
    "Insert a blank disc": "Włóż pusty dysk",
    "Insert a rewritable disc": "Włóż dysk wielokrotnego zapisu",
    "Insufficient disc space": "Niewystarczające miejsce na dysku",
    "Required: %1, Available: %2": "Wymagane: %1, Dostępne: %2",

    # Menu items
    "&File": "&Plik",
    "&Edit": "&Edycja",
    "&View": "&Widok",
    "&Tools": "&Narzędzia",
    "&Help": "&Pomoc",
    "&New Project": "&Nowy projekt",
    "&Open Project...": "&Otwórz projekt...",
    "&Save Project": "&Zapisz projekt",
    "Save Project &As...": "Zapisz projekt &jako...",
    "&Recent Projects": "&Ostatnie projekty",
    "E&xit": "&Zakończ",
    "&Undo": "&Cofnij",
    "&Redo": "&Ponów",
    "Cu&t": "W&ytnij",
    "&Copy": "&Kopiuj",
    "&Paste": "&Wklej",
    "&Preferences...": "&Preferencje...",
    "&Refresh Drive List": "&Odśwież listę napędów",
    "&Eject Disc": "&Wysuń dysk",
    "&Load Disc": "&Załaduj dysk",
    "&Blank Disc...": "&Wyczyść dysk...",
    "&Disc Info": "&Informacje o dysku",
    "&About": "&O programie",
    "About &Qt": "O &Qt",

    # About dialog
    "About Burner": "O Burner",
    "Version %1": "Wersja %1",
    "A simple CD/DVD/Blu-ray burning application": "Prosta aplikacja do nagrywania CD/DVD/Blu-ray",
    "Built with Qt %1": "Zbudowano z Qt %1",
    "Copyright © 2025": "Copyright © 2025",

    # Drive selector
    "No drives detected": "Nie wykryto napędów",
    "Select a drive": "Wybierz napęd",
    "Drive:": "Napęd:",
    "No disc": "Brak dysku",
    "Empty": "Pusty",
    "Not empty": "Nie pusty",
    "Audio CD": "Audio CD",
    "Data disc": "Dysk danych",
    "Blank": "Pusty",
    "Rewritable": "Wielokrotnego zapisu",
    "Capacity: %1": "Pojemność: %1",
    "Free: %1": "Wolne: %1",
    "Used: %1": "Użyte: %1",

    # File formats
    "All Files": "Wszystkie pliki",
    "ISO Image Files": "Pliki obrazu ISO",
    "Audio Files": "Pliki dźwiękowe",
    "Video Files": "Pliki wideo",
    "Image Files": "Pliki obrazu",

    # Tooltips and help
    "Add files and folders to burn to a data disc": "Dodaj pliki i foldery do nagrania na dysk danych",
    "Burn an ISO image file to disc": "Nagraj plik obrazu ISO na dysk",
    "Create an audio CD from music files": "Utwórz Audio CD z plików muzycznych",
    "Create a Video CD from video files": "Utwórz Video CD z plików wideo",
    "Copy a disc or create disc image": "Skopiuj dysk lub utwórz obraz dysku",
    "Rip audio CD to files": "Zgraj Audio CD do plików",

    # Blanking
    "Blank Disc": "Wyczyść dysk",
    "Quick blank (erase TOC only)": "Szybkie czyszczenie (tylko TOC)",
    "Full blank (erase entire disc)": "Pełne czyszczenie (cały dysk)",
    "Blank Type": "Typ czyszczenia",
    "Quick": "Szybkie",
    "Full": "Pełne",

    # Cloning
    "Copy disc to disc": "Kopiuj dysk na dysk",
    "Copy disc to image file": "Kopiuj dysk do pliku obrazu",
    "Burn image file to disc": "Nagraj plik obrazu na dysk",
    "Source drive:": "Napęd źródłowy:",
    "Target drive:": "Napęd docelowy:",
    "Image file:": "Plik obrazu:",
    "On-the-fly copy": "Kopiowanie w locie",
    "Copy without creating temporary image": "Kopiuj bez tworzenia tymczasowego obrazu",

    # Ripping
    "Output format:": "Format wyjściowy:",
    "Bitrate:": "Bitrate:",
    "Quality:": "Jakość:",
    "Output directory:": "Katalog wyjściowy:",
    "Filename pattern:": "Wzorzec nazwy pliku:",
    "Album artist": "Artysta albumu",
    "Album title": "Tytuł albumu",
    "Year": "Rok",
    "Genre": "Gatunek",
    "Track number": "Numer utworu",
    "Track title": "Tytuł utworu",
    "Track artist": "Artysta utworu",
    "Disc number": "Numer dysku",
    "Total tracks": "Wszystkich utworów",
    "Rip selected tracks": "Zgraj zaznaczone utwory",
    "Rip all tracks": "Zgraj wszystkie utwory",
    "Stop ripping": "Zatrzymaj zgrywanie",

    # Common patterns
    "Select %1": "Wybierz %1",
    "Selected: %1 of %2": "Zaznaczono: %1 z %2",
    "%1 MB": "%1 MB",
    "%1 GB": "%1 GB",
    "%1 bytes": "%1 bajtów",
    "%1 files": "%1 plików",
    "%1 folders": "%1 folderów",

    # Additional UI translations
    "Select Audio Files": "Wybierz pliki dźwiękowe",
    "DISC": "DYSK",
    "Simulate (dry run - don't actually burn)": "Symuluj (test bez nagrywania)",
    "Enable buffer underrun protection": "Włącz ochronę przed opróżnieniem bufora",
    "Blank rewritable disc before burning": "Wyczyść dysk wielokrotnego zapisu przed nagraniem",
    "Read: --": "Odczyt: --",
    "Read: ": "Odczyt: ",
    "%1%2x DVD (%3 KB/s)": "%1%2x DVD (%3 KB/s)",
    "Clone Failed": "Klonowanie nie powiodło się",
    "Burn completed successfully!": "Nagrywanie zakończone pomyślnie!",
    "Cancel Clone?": "Anulować klonowanie?",
    "Are you sure you want to cancel the burn operation?": "Czy na pewno chcesz anulować operację nagrywania?",
    "Drop files or folders here\nto add them to the disc": "Upuść pliki lub foldery tutaj\naby dodać je do dysku",
    "Drop files or folders here": "Upuść pliki lub foldery tutaj",
    "Save as ISO...": "Zapisz jako ISO...",
    "All Files (*)": "Wszystkie pliki (*)",
    "Save ISO Image": "Zapisz obraz ISO",
    "Create an exact copy of a disc as an ISO image file.\nUseful for backing up discs or distributing disc images.": "Utwórz dokładną kopię dysku jako plik obrazu ISO.\nPrzydatne do tworzenia kopii zapasowych dysków lub dystrybucji obrazów dysków.",
    "Create an exact copy of a disc as an ISO image file.": "Utwórz dokładną kopię dysku jako plik obrazu ISO.",
    "Save Disc Image": "Zapisz obraz dysku",
    "Refresh": "Odśwież",
    "Eject disc": "Wysuń dysk",
    "Burn ISO Image to Disc": "Nagraj obraz ISO na dysk",
    "ISO File:": "Plik ISO:",
    "Clear": "Wyczyść",
    "Size:": "Rozmiar:",
    "Select ISO Image": "Wybierz obraz ISO",
    "(none)": "(brak)",

    # Main Window
    "&Data Disc": "&Dysk danych",
    "&ISO Writer": "&Zapis ISO",
    "&Audio CD": "&Audio CD",
    "&Video CD": "&Video CD",
    "Disc &Cloner": "&Klonowanie dysku",
    "CD &Ripper": "&Zgry wanie CD",
    "Data Disc": "Dysk danych",
    "ISO Writer": "Zapis ISO",
    "Video CD": "Video CD",
    "Disc Cloner": "Klonowanie dysku",
    "CD Ripper": "Zgrywanie CD",
    "Burn data files and folders to CD/DVD/Blu-ray": "Nagraj pliki i foldery danych na CD/DVD/Blu-ray",
    "Write an ISO image file to disc": "Zapisz plik obrazu ISO na dysk",
    "Create an audio CD from music files (MP3, FLAC, WAV, etc.)": "Utwórz Audio CD z plików muzycznych (MP3, FLAC, WAV itp.)",
    "Create a Video CD (VCD/SVCD) from video files": "Utwórz Video CD (VCD/SVCD) z plików wideo",
    "Clone a disc or create/burn disc images": "Sklonuj dysk lub utwórz/nagraj obrazy dysków",
    "Rip audio CD tracks to files (MP3, FLAC, WAV, etc.)": "Zgraj utwory Audio CD do plików (MP3, FLAC, WAV itp.)",

    # Burn Options Dialog
    "Burn Options": "Opcje nagrywania",
    "Write Speed:": "Prędkość zapisu:",
    "Burn settings": "Ustawienia nagrywania",
    "Post-burn actions": "Działania po nagraniu",
    "Advanced options": "Opcje zaawansowane",
    "Write Method:": "Metoda zapisu:",
    "Auto-detect": "Automatycznie wykryj",
    "Track-At-Once": "Track-At-Once",
    "Session-At-Once": "Session-At-Once",
    "Disc-At-Once": "Disc-At-Once",
    "RAW96R (Raw writing)": "RAW96R (zapis surowy)",

    # Burn Progress Dialog
    "Burn Progress": "Postęp nagrywania",
    "Operation:": "Operacja:",
    "Status:": "Status:",
    "Progress:": "Postęp:",
    "Time remaining:": "Pozostały czas:",
    "Current speed:": "Obecna prędkość:",
    "Hide Details": "Ukryj szczegóły",
    "Show Details": "Pokaż szczegóły",
    "Stop": "Zatrzymaj",
    "Details": "Szczegóły",
    "Show log details": "Pokaż szczegóły dziennika",
    "Hide log details": "Ukryj szczegóły dziennika",
    "Estimated time: %1": "Szacowany czas: %1",
    "Time elapsed: %1": "Upłynęło: %1",
    "Writing at %1x speed": "Zapisywanie z prędkością %1x",
    "Buffer %1%": "Bufor %1%",

    # Data Disc Page
    "Drop files and folders here": "Upuść pliki i foldery tutaj",
    "Add files and folders to create a data disc": "Dodaj pliki i foldery aby utworzyć dysk danych",
    "New Folder": "Nowy folder",
    "Enter folder name:": "Wprowadź nazwę folderu:",
    "Folder name:": "Nazwa folderu:",
    "A folder with this name already exists": "Folder o tej nazwie już istnieje",
    "Rename Item": "Zmień nazwę elementu",
    "New name:": "Nowa nazwa:",
    "Are you sure you want to delete the selected items?": "Czy na pewno chcesz usunąć zaznaczone elementy?",
    "Delete Items": "Usuń elementy",
    "Calculate": "Oblicz",

    # Disc Cloner Page
    "Disc Cloning": "Klonowanie dysku",
    "Clone disc directly (on-the-fly)": "Klonuj dysk bezpośrednio (w locie)",
    "Copy source disc directly to target drive without creating an intermediate image file. Both drives must be available.": "Kopiuj dysk źródłowy bezpośrednio do napędu docelowego bez tworzenia pośredniego pliku obrazu. Oba napędy muszą być dostępne.",
    "Create disc image file": "Utwórz plik obrazu dysku",
    "Save to:": "Zapisz do:",
    "Burn from disc image": "Nagraj z obrazu dysku",
    "Read from:": "Odczytaj z:",
    "Start Clone": "Rozpocznij klonowanie",
    "Create Image": "Utwórz obraz",
    "Burn Image": "Nagraj obraz",

    # ISO Writer Page
    "Write an ISO image file to a recordable disc. The ISO image must be bootable if you want to create a bootable disc.": "Zapisz plik obrazu ISO na dysk nagrywalny. Obraz ISO musi być bootowalny, jeśli chcesz utworzyć dysk bootowalny.",
    "Write an ISO image file to a recordable disc.": "Zapisz plik obrazu ISO na dysk nagrywalny.",
    "Image information": "Informacje o obrazie",
    "File:": "Plik:",
    "Not selected": "Nie wybrano",
    "MD5:": "MD5:",
    "SHA256:": "SHA256:",
    "Calculating...": "Obliczanie...",
    "Calculate checksums": "Oblicz sumy kontrolne",

    # Video CD Page
    "Drop video files here\n(MP4, AVI, MKV, etc.)": "Upuść pliki wideo tutaj\n(MP4, AVI, MKV itp.)",
    "Add Videos...": "Dodaj filmy...",
    "Format:": "Format:",
    "Total: 0:00 / 60:00 (VCD)": "Całkowity: 0:00 / 60:00 (VCD)",
    "VCD": "VCD",
    "SVCD": "SVCD",

    # Drive Selector
    "Scanning for drives...": "Skanowanie napędów...",
    "Click to refresh drive list": "Kliknij aby odświeżyć listę napędów",
    "Click to eject disc": "Kliknij aby wysunąć dysk",

    # File Drop Area
    "Click or drag files here": "Kliknij lub przeciągnij pliki tutaj",

    # Additional common terms
    "Untitled": "Bez tytułu",
    "Unknown": "Nieznany",
    "None": "Brak",
    "Default": "Domyślny",
    "Custom": "Niestandardowy",
    "seconds": "sekundy",
    "minutes": "minuty",
    "hours": "godziny",
    "Loading...": "Ładowanie...",
    "Please wait...": "Proszę czekać...",
    "Done": "Gotowe",
    "Success": "Sukces",
    "Aborted": "Przerwano",
    "Invalid": "Nieprawidłowy",
    "Unknown error": "Nieznany błąd",
    "Not available": "Niedostępny",
    "Enabled": "Włączony",
    "Disabled": "Wyłączony",
    "Automatic": "Automatyczny",
    "Manual": "Ręczny",

    # More UI translations
    "Warning: Total duration exceeds 80 minutes!": "Ostrzeżenie: Całkowity czas przekracza 80 minut!",
    "Total: %1:%2 / 80:00": "Całkowity: %1:%2 / 80:00",
    "Volume Label:": "Etykieta woluminu:",
    "Write Speed:": "Prędkość zapisu:",
    "Verify disc after burning": "Weryfikuj dysk po nagraniu",
    "Leave disc open for additional sessions": "Pozostaw dysk otwarty dla dodatkowych sesji",
    "If checked, the disc can have more data added later.": "Jeśli zaznaczone, można później dodać więcej danych na dysk.",
    "Burning Disc": "Nagrywanie dysku",
    "Elapsed: %1 | Remaining: %2": "Upłynęło: %1 | Pozostało: %2",
    "Speed: ": "Prędkość: ",
    "%1%2x CD (%3 KB/s)": "%1%2x CD (%3 KB/s)",
    "Burn Complete": "Nagrywanie zakończone",
    "Burn Failed": "Nagrywanie nie powiodło się",
    "Clone completed successfully!": "Klonowanie zakończone pomyślnie!",
    "Burn failed: %1": "Nagrywanie nie powiodło się: %1",
    "Cancel Burn?": "Anulować nagrywanie?",
    "Are you sure you want to cancel the clone operation?": "Czy na pewno chcesz anulować operację klonowania?",
    "Preparing to burn...": "Przygotowywanie do nagrywania...",
    "Data CD/DVD": "CD/DVD danych",
    "Save disc contents to an ISO file": "Zapisz zawartość dysku do pliku ISO",
    "Select Files": "Wybierz pliki",
    "ISO Images (*.iso);;All Files (*)": "Obrazy ISO (*.iso);;Wszystkie pliki (*)",
    "Source Drive:": "Napęd źródłowy:",
    "Select output file location...": "Wybierz lokalizację pliku wyjściowego...",
    "Clone Disc": "Klonuj dysk",
    "Scan for drives": "Skanuj napędy",
    "Eject": "Wysuń",
    "Appendable": "Możliwy do rozszerzenia",
    "Mounted (unmount to burn)": "Zamontowany (odmontuj aby nagrać)",
    "Drop files here": "Upuść pliki tutaj",
    "Drag and drop an ISO file here, or use the Browse button below": "Przeciągnij i upuść plik ISO tutaj lub użyj przycisku Przeglądaj poniżej",

    # More patterns and contexts
    "Write ISO image to disc": "Zapisz obraz ISO na dysk",
    "Create bootable disc from ISO": "Utwórz dysk bootowalny z ISO",
    "Burn image...": "Nagraj obraz...",
    "Cancel burn operation": "Anuluj operację nagrywania",
    "Verify data integrity": "Weryfikuj integralność danych",
    "Multi-session disc": "Dysk wielosesyjny",
    "Single session": "Pojedyncza sesja",
    "Joliet extensions": "Rozszerzenia Joliet",
    "Rock Ridge extensions": "Rozszerzenia Rock Ridge",
    "Follow symbolic links": "Podążaj za dowiązaniami symbolicznymi",
    "Include hidden files": "Dołącz ukryte pliki",
    "Disc label": "Etykieta dysku",
    "Creation date": "Data utworzenia",
    "Modification date": "Data modyfikacji",
    "Boot image": "Obraz rozruchowy",
    "Catalog file": "Plik katalogu",
    "System identifier": "Identyfikator systemu",
    "Volume set": "Zestaw woluminów",
    "Copyright file": "Plik copyright",
    "Abstract file": "Plik abstrakcyjny",
    "Bibliographic file": "Plik bibliograficzny",

    # MainWindow items
    "Burner": "Burner",
    "Ready to burn": "Gotowy do nagrywania",
    "No burner drives detected": "Nie wykryto napędów do nagrywania",
    "Insert a disc to begin": "Włóż dysk aby rozpocząć",
    "Checking drives...": "Sprawdzanie napędów...",
    "Drive not ready": "Napęd nie gotowy",
    "Unsupported disc type": "Nieobsługiwany typ dysku",
    "Disc is write-protected": "Dysk jest chroniony przed zapisem",
    "Insufficient permissions": "Niewystarczające uprawnienia",
    "Device busy": "Urządzenie zajęte",
    "Hardware error": "Błąd sprzętowy",
    "Buffer underrun": "Opróżnienie bufora",
    "Write error": "Błąd zapisu",
    "Read error": "Błąd odczytu",
    "Verification failed": "Weryfikacja nie powiodła się",
    "Disc full": "Dysk pełny",
    "Session closed": "Sesja zamknięta",
    "Invalid session": "Nieprawidłowa sesja",
    "Track mode mismatch": "Niezgodność trybu ścieżki",
    "Unsupported format": "Nieobsługiwany format",
    "Encoding error": "Błąd kodowania",
    "Decoding error": "Błąd dekodowania",
    "File format error": "Błąd formatu pliku",
    "Codec not found": "Nie znaleziono kodeka",
    "Invalid bitrate": "Nieprawidłowy bitrate",
    "Invalid sample rate": "Nieprawidłowa częstotliwość próbkowania",
    "Channel configuration error": "Błąd konfiguracji kanałów",
    "Timestamp error": "Błąd znacznika czasu",
    "Synchronization error": "Błąd synchronizacji",
    "Metadata error": "Błąd metadanych",
    "Checksum mismatch": "Niezgodność sumy kontrolnej",
    "CRC error": "Błąd CRC",
    "Parity error": "Błąd parzystości",
    "ECC failed": "ECC nie powiodło się",

    # Specific Main Window menu and toolbar items
    "New": "Nowy",
    "Open": "Otwórz",
    "Save": "Zapisz",
    "Save As": "Zapisz jako",
    "Recent": "Ostatnie",
    "Exit": "Zakończ",
    "Quit": "Zakończ",
    "Undo": "Cofnij",
    "Redo": "Ponów",
    "Cut": "Wytnij",
    "Copy": "Kopiuj",
    "Paste": "Wklej",
    "Preferences": "Preferencje",
    "Settings": "Ustawienia",
    "Options": "Opcje",
    "Tools": "Narzędzia",
    "Help": "Pomoc",
    "About": "O programie",
    "Documentation": "Dokumentacja",
    "Report Bug": "Zgłoś błąd",
    "Check for Updates": "Sprawdź aktualizacje",
    "License": "Licencja",
    "Credits": "Autorzy",
    "Donate": "Przekaż darowiznę",
    "Website": "Strona WWW",
    "Support": "Wsparcie",
    "Forum": "Forum",
    "Contact": "Kontakt",

    # Additional patterns with apostrophes and special characters
    "don&apos;t": "nie",
    "can&apos;t": "nie można",
    "won&apos;t": "nie będzie",
    "isn&apos;t": "nie jest",
    "aren&apos;t": "nie są",
    "wasn&apos;t": "nie był",
    "weren&apos;t": "nie byli",
    "haven&apos;t": "nie mają",
    "hasn&apos;t": "nie ma",
    "hadn&apos;t": "nie miał",
    "doesn&apos;t": "nie",
    "didn&apos;t": "nie",
    "couldn&apos;t": "nie mógł",
    "shouldn&apos;t": "nie powinien",
    "wouldn&apos;t": "nie chciałby",
    "mightn&apos;t": "mógłby nie",
    "mustn&apos;t": "nie wolno",

    # More specific UI strings
    "Select or drop an ISO file...": "Wybierz lub upuść plik ISO...",
    "ISO Image Information": "Informacje o obrazie ISO",
    "Type:": "Typ:",
    "Recommended Disc:": "Zalecany dysk:",
    "File not found: %1": "Nie znaleziono pliku: %1",
    "Nero Image": "Obraz Nero",
    "BIN/CUE Image": "Obraz BIN/CUE",
    "ISO 9660 Image": "Obraz ISO 9660",
    "CD (700 MB)": "CD (700 MB)",
    "DVD (4.7 GB)": "DVD (4.7 GB)",
    "DVD DL (8.5 GB)": "DVD DL (8.5 GB)",
    "BD (25 GB)": "BD (25 GB)",
    "BD DL (50 GB)": "BD DL (50 GB)",
    "BD XL (100+ GB)": "BD XL (100+ GB)",
    "Invalid or unsupported image format": "Nieprawidłowy lub nieobsługiwany format obrazu",
    "Burner - CD/DVD Burning Application": "Burner - Aplikacja do nagrywania CD/DVD",
    "Initialization Error": "Błąd inicjalizacji",
    "Failed to initialize burning engine:": "Nie udało się zainicjalizować silnika nagrywania:",
    "Burn ISO": "Nagraj ISO",
    "&New Project": "&Nowy projekt",
    "&Open Project...": "&Otwórz projekt...",
    "&Save Project": "&Zapisz projekt",
    "Save Project &As...": "Zapisz projekt &jako...",
    "E&xit": "&Zakończ",
    "&Tools": "&Narzędzia",
    "&Refresh Drives": "&Odśwież napędy",
    "&Blank Disc": "&Wyczyść dysk",
    "&Eject": "&Wysuń",
    "&Load Disc": "&Załaduj dysk",
    "&About Burner": "&O programie Burner",
    "About &Qt": "O &Qt",
    "Visit &Website": "Odwiedź &stronę WWW",

    # Specific long descriptions
    "Useful for backing up discs or distributing disc images.": "Przydatne do tworzenia kopii zapasowych dysków lub dystrybucji obrazów dysków.",
    "Copy source disc directly to target drive without creating an intermediate image file. Both drives must be available.": "Kopiuj dysk źródłowy bezpośrednio do napędu docelowego bez tworzenia pośredniego pliku obrazu. Oba napędy muszą być dostępne.",
    "The ISO image must be bootable if you want to create a bootable disc.": "Obraz ISO musi być bootowalny, jeśli chcesz utworzyć dysk bootowalny.",

    # Additional common
    "Track": "Utwór",
    "Tracks": "Utwory",
    "Album": "Album",
    "Disc": "Dysk",
    "Media": "Nośnik",
    "Session": "Sesja",
    "Lead-in": "Lead-in",
    "Lead-out": "Lead-out",
    "Gap": "Przerwa",
    "Index": "Indeks",
    "ISRC": "ISRC",
    "MCN": "MCN",
    "UPC": "UPC",
    "EAN": "EAN",
    "Barcode": "Kod kreskowy",
    "Catalog": "Katalog",
    "TOC": "TOC",
    "PMA": "PMA",
    "ATIP": "ATIP",
    "Multiborder": "Wielograniczny",

    # Encoding and format options
    "MP3": "MP3",
    "FLAC": "FLAC",
    "WAV": "WAV",
    "OGG": "OGG",
    "AAC": "AAC",
    "WMA": "WMA",
    "M4A": "M4A",
    "ALAC": "ALAC",
    "APE": "APE",
    "WV": "WV",
    "Lossless": "Bezstratny",
    "Lossy": "Stratny",
    "CBR": "CBR",
    "VBR": "VBR",
    "ABR": "ABR",

    # Video formats
    "MP4": "MP4",
    "AVI": "AVI",
    "MKV": "MKV",
    "MOV": "MOV",
    "WMV": "WMV",
    "FLV": "FLV",
    "MPEG": "MPEG",
    "MPG": "MPG",
    "M2V": "M2V",
    "VOB": "VOB",

    # Additional status and error messages
    "Preparing...": "Przygotowywanie...",
    "Processing...": "Przetwarzanie...",
    "Analyzing...": "Analiza...",
    "Optimizing...": "Optymalizacja...",
    "Finalizing...": "Finalizacja...",
    "Cleaning up...": "Czyszczenie...",
    "Complete": "Ukończono",
    "Incomplete": "Nieukończono",
    "Partial": "Częściowo",
    "Skipped": "Pominięto",
    "Ignored": "Zignorowano",
    "Overwrite": "Nadpisz",
    "Append": "Dołącz",
    "Merge": "Scal",
    "Split": "Podziel",
    "Convert": "Konwertuj",
    "Extract": "Wyodrębnij",
    "Compress": "Kompresuj",
    "Decompress": "Dekompresuj",
    "Encrypt": "Zaszyfruj",
    "Decrypt": "Odszyfruj",
    "Sign": "Podpisz",
    "Verify signature": "Weryfikuj podpis",
    "Hash": "Hash",
    "Checksum": "Suma kontrolna",

    # Final batch of remaining translations
    "%1 files, %2 MB total": "%1 plików, %2 MB łącznie",
    "%1 tracks, %2:%3 total duration": "%1 utworów, %2:%3 całkowity czas",
    "%1 videos": "%1 filmów",
    "About Burner": "O programie Burner",
    "Audio CD - Add audio tracks (MP3, FLAC, WAV)": "Audio CD - Dodaj utwory dźwiękowe (MP3, FLAC, WAV)",
    "Burner Project (*.burn);;All Files (*)": "Projekt Burner (*.burn);;Wszystkie pliki (*)",
    "Burn ISO - Burn an ISO image file": "Nagraj ISO - Nagraj plik obrazu ISO",
    "Change Project Type": "Zmień typ projektu",
    "Clone Disc - Copy disc to image or image to disc": "Klonuj dysk - Kopiuj dysk do obrazu lub obraz na dysk",
    "Cloning Disc": "Klonowanie dysku",
    "Could not save file: %1": "Nie można zapisać pliku: %1",
    "Creating ISO Image": "Tworzenie obrazu ISO",
    "Data CD/DVD - Add files and folders to burn": "CD/DVD danych - Dodaj pliki i foldery do nagrania",
    "Data Too Large": "Dane za duże",
    "Drop video files here": "Upuść pliki wideo tutaj",
    "Enter volume label for the ISO:": "Wprowadź etykietę woluminu dla ISO:",
    "Image: %1": "Obraz: %1",
    "Invalid project file: %1": "Nieprawidłowy plik projektu: %1",
    "ISO Image": "Obraz ISO",
    "No Drive Selected": "Nie wybrano napędu",
    "No Files": "Brak plików",
    "No Output File": "Brak pliku wyjściowego",
    "No Source Drive": "Brak napędu źródłowego",
    "Not Implemented": "Nie zaimplementowano",
    "No Videos": "Brak filmów",
    "Open Project": "Otwórz projekt",
    "Please add files to create an ISO image.": "Proszę dodać pliki aby utworzyć obraz ISO.",
    "Please add video files to burn.": "Proszę dodać pliki wideo do nagrania.",
    "Please select a drive to burn to.": "Proszę wybrać napęd do nagrywania.",
    "Please select a source drive to clone.": "Proszę wybrać napęd źródłowy do klonowania.",
    "Please specify an output file path for the disc image.": "Proszę określić ścieżkę pliku wyjściowego dla obrazu dysku.",
    "Save Project As": "Zapisz projekt jako",
    "Simulation Mode Warning": "Ostrzeżenie trybu symulacji",
    "The data (%1 MB) is too large for the disc (%2 MB available).": "Dane (%1 MB) są za duże dla dysku (dostępne %2 MB).",
    "The disc in %1 is currently mounted by the system.": "Dysk w %1 jest obecnie zamontowany przez system.",
    "The project has unsaved changes. Do you want to save them?": "Projekt ma niezapisane zmiany. Czy chcesz je zapisać?",
    "This burn mode is not yet implemented.": "Ten tryb nagrywania nie jest jeszcze zaimplementowany.",
    "Use the 'Clone Disc' button on the Clone tab to start cloning.": "Użyj przycisku 'Klonuj dysk' w zakładce Klonowanie aby rozpocząć klonowanie.",
    "VCD 2.0": "VCD 2.0",
    "VCD: MPEG-1 352x288 (PAL) / 352x240 (NTSC) - Up to 80 minutes | SVCD: MPEG-2 480x576 (PAL) / 480x480 (NTSC) - Up to 60 minutes": "VCD: MPEG-1 352x288 (PAL) / 352x240 (NTSC) - Do 80 minut | SVCD: MPEG-2 480x576 (PAL) / 480x480 (NTSC) - Do 60 minut",
    "Video CD (%1)": "Video CD (%1)",
    "Video CD - Add videos for VCD/SVCD": "Video CD - Dodaj filmy dla VCD/SVCD",
    "Video Files (*.mp4 *.avi *.mkv *.mov *.wmv *.mpg *.mpeg *.m4v *.webm);;All Files (*)": "Pliki wideo (*.mp4 *.avi *.mkv *.mov *.wmv *.mpg *.mpeg *.m4v *.webm);;Wszystkie pliki (*)",

    # Final remaining translations
    "Audio Files (*.mp3 *.flac *.wav *.ogg *.aac *.m4a *.wma);;All Files (*)": "Pliki dźwiękowe (*.mp3 *.flac *.wav *.ogg *.aac *.m4a *.wma);;Wszystkie pliki (*)",
    "Blank %1": "Wyczyść %1",
    "Buffer:": "Bufor:",
    "Burn Summary": "Podsumowanie nagrywania",
    "Clone Complete": "Klonowanie ukończone",
    "Clone Disc to Image": "Klonuj dysk do obrazu",
    "Clone failed: %1": "Klonowanie nie powiodło się: %1",
    "Could not open file: %1": "Nie można otworzyć pliku: %1",
    "Disc Type:": "Typ dysku:",
    "Drive is Mounted": "Napęd jest zamontowany",
    "Ejecting disc...": "Wysuwanie dysku...",
    "Failed to load project data.": "Nie udało się załadować danych projektu.",
    "ISO Images (*.iso *.img *.bin *.nrg);;All Files (*)": "Obrazy ISO (*.iso *.img *.bin *.nrg);;Wszystkie pliki (*)",
    "<h2>Burner</h2><p>CD/DVD/Blu-ray Burning Application</p><p>Version 1.0</p><p>Built with Qt6 and libburnia (libburn, libisofs)</p><p>libburn version: %1</p><p>libisofs version: %2</p>": "<h2>Burner</h2><p>Aplikacja do nagrywania CD/DVD/Blu-ray</p><p>Wersja 1.0</p><p>Zbudowano z Qt6 i libburnia (libburn, libisofs)</p><p>Wersja libburn: %1</p><p>Wersja libisofs: %2</p>",
    "Main": "Główne",
    "Output File:": "Plik wyjściowy:",
    "Project saved": "Projekt zapisany",
    "Select an ISO image file to burn directly to disc. The disc will be an exact copy of the ISO image.": "Wybierz plik obrazu ISO do nagrania bezpośrednio na dysk. Dysk będzie dokładną kopią obrazu ISO.",
    "Select Folder": "Wybierz folder",
    "Select Video Files": "Wybierz pliki wideo",
    "Speed: --": "Prędkość: --",
    "Start burning disc": "Rozpocznij nagrywanie dysku",
    "Unsaved Changes": "Niezapisane zmiany",
    "For CD-RW, DVD-RW, DVD+RW, or BD-RE discs:": "Dla dysków CD-RW, DVD-RW, DVD+RW lub BD-RE:",

    # Final multiline strings
    "If checked, the disc can have more data added later.\nIf unchecked (default), the disc will be finalized and closed.": "Jeśli zaznaczone, można później dodać więcej danych na dysk.\nJeśli odznaczone (domyślnie), dysk zostanie sfinalizowany i zamknięty.",
    "For CD-RW, DVD-RW, DVD+RW, or BD-RE discs:\nErase existing data before writing new content.": "Dla dysków CD-RW, DVD-RW, DVD+RW lub BD-RE:\nWymaż istniejące dane przed zapisaniem nowej zawartości.",
    "Are you sure you want to cancel the burn operation?\nThe disc may be unusable.": "Czy na pewno chcesz anulować operację nagrywania?\nDysk może być nieużywalny.",
    "Drop files or folders here\nto add to disc": "Upuść pliki lub foldery tutaj\naby dodać do dysku",
    "Create an exact copy of a disc as an ISO image file.\nThe disc must be unmounted before cloning.": "Utwórz dokładną kopię dysku jako plik obrazu ISO.\nDysk musi być odmontowany przed klonowaniem.",
    "Drop files here\nor use Add button": "Upuść pliki tutaj\nlub użyj przycisku Dodaj",
    "Image: %1\nSize: %2 MB": "Obraz: %1\nRozmiar: %2 MB",
    "The data (%1 MB) is too large for the disc (%2 MB available).\n\nPlease remove some files or use a larger disc.": "Dane (%1 MB) są za duże dla dysku (dostępne %2 MB).\n\nProszę usunąć niektóre pliki lub użyć większego dysku.",
    "Drop video files here\n(MP4, AVI, MKV, MOV, MPG)": "Upuść pliki wideo tutaj\n(MP4, AVI, MKV, MOV, MPG)",
}

def translate_text(text):
    """Translate English text to Polish"""
    # Direct lookup
    if text in TRANSLATIONS:
        return TRANSLATIONS[text]

    # Try without trailing ellipsis
    if text.endswith('...'):
        base = text[:-3]
        if base in TRANSLATIONS:
            return TRANSLATIONS[base] + '...'

    # Handle HTML entities
    text_decoded = text.replace('&apos;', "'").replace('&amp;', '&').replace('&lt;', '<').replace('&gt;', '>')
    if text_decoded != text and text_decoded in TRANSLATIONS:
        result = TRANSLATIONS[text_decoded]
        return result.replace("'", '&apos;').replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

    # Return original if no translation found
    return None

def process_file(filepath):
    """Process the translation file and update unfinished translations"""
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    translated_count = 0
    i = 0
    while i < len(lines):
        # Check if this is an unfinished translation line
        if '<translation type="unfinished"></translation>' in lines[i]:
            # Look backwards for the source tag (up to 5 lines)
            source_text = None
            for j in range(max(0, i-5), i):
                if '<source>' in lines[j]:
                    source_match = re.search(r'<source>(.*?)</source>', lines[j])
                    if source_match:
                        source_text = source_match.group(1)
                        break

            if source_text:
                translation = translate_text(source_text)
                if translation:
                    # Replace the unfinished translation with the translated version
                    lines[i] = lines[i].replace(
                        '<translation type="unfinished"></translation>',
                        f'<translation>{translation}</translation>'
                    )
                    translated_count += 1
        i += 1

    # Write back
    with open(filepath, 'w', encoding='utf-8') as f:
        f.writelines(lines)

    # Count remaining unfinished
    remaining = sum(1 for line in lines if 'type="unfinished"' in line)

    print(f"Translated: {translated_count}")
    print(f"Remaining unfinished: {remaining}")

    return translated_count, remaining

if __name__ == '__main__':
    filepath = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'translations', 'burner_pl.ts')
    translated, remaining = process_file(filepath)

    if remaining == 0:
        print("\n✓ All translations completed!")
        sys.exit(0)
    else:
        print(f"\n⚠ {remaining} translations still need manual review")
        sys.exit(0)
