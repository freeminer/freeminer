## Script for automatic building of freeminer for OS X from GitHub

Originally was here https://github.com/mdoege/mtmake-osx

### Usage

Start Terminal.app (or any other terminal emulator for OS X) and install all dependencies (see below). It is recommended to use Homebrew for this, except for e.g. Xcode which you can download from Apple's Mac App Store for free.

Then clone the mtmake-osx build scripts from GitHub and start the compilation process like this:

    git clone --recursive https://github.com/freeminer/freeminer
    cd build/osx
    bash make_mac.sh

This will compile freeminer, updating the app in releases/ and creating a ZIP file with the app bundle.

### Downloading mtmake-osx from GitHub as a ZIP file

(This section only applies if you cannot clone mtmake-osx with git for some reason.)

You cannot just go to the GitHub page and right click the script and do "save as...". This just saves the html page with the script. The entire folder provided on the GitHub page is required. Download the [zip file](https://github.com/mdoege/mtmake-osx/archive/master.zip), extract it, navigate inside it with a terminal and type ./make_mac.sh. The zip file with the application can be found in the "releases" folder.

### How it works

The script

* updates freeminer OS X executable and shared files
* does not change libraries (which is why a previous version of freeminer.app with its modified dynamic libraries is included)

### Dependencies

Install these pacakges with [Homebrew](http://brew.sh/) ("brew install cmake freetype gettext hiredis irrlicht jpeg leveldb libogg libvorbis luajit msgpack")

(snappy and libpng will get installed by brew automatically too.)

You need Xcode 5 and the Xcode Command Line Tools. You should get prompted for installation of the latter if you run the build script for the first time. Alternatively, start Xcode, go to Xcode->Preferences->Downloads and install a component named "Command Line Tools".

You will also have to install [XQuartz](http://xquartz.macosforge.org/) for X11 support.

If git is not installed, you can get it via Homebrew too: "brew install git"

Note that for compatibility with older CPUs (like Core 2) it is necessary to install
dependencies from bottles, because those are automatically compiled by Homebrew
to work on all CPUs supported by OS X Mavericks.

### Playing the game

* Use two finger tap for right click

* Use "e" for sneak/climb down (activate in 'Settings -> Change keys')

* If you would like to start the freeminer server from a terminal, run "/Applications/freeminer.app/Contents/Resources/bin/freeminer --server".

### How to update dynamic libraries (rarely needed)

The build_libs.sh script (in releases/) will rebuild the libs folder. This means that dynamic libraries which the MT executable depends on will get copied into the bundle directory.

Normally this should only be necessary if the libraries your executable got linked to have a different version number than the ones already in the bundle. Build_libs.sh will copy the libraries from e.g. the Homebrew folder (/usr/local/Cellar/) into the bundle and change their install names, so that every library in the bundle will match the globally installed libraries on your system.

Note that build_libs.sh needs an existing MT binary in freeminer-git/bin/ to scan for its dynamic library dependencies! So first you need to build MT itself successfully before you can then rebuild the libs folder.

Also, don't forget to create an updated ZIP file with update_zip.sh after running build_libs.sh if you intend to distribute the ZIP file!

Below are some explanations for manually updating libraries without the use of build_libs.sh. Normally this should not be needed but it explains what build_libs.sh does.

This command will create the libs directory and change library path names:

    dylibbundler -x freeminer.app/Contents/Resources/bin/freeminer -b -d ./freeminer.app/Contents/libs/ \
    -p @executable_path/../../libs/ -cd

(Note that this only works with an unmodified minetet binary that has not had its own link s changed by e.g. the dylibbundler invocation in make_mac.sh itself.)

Check if all no libraries point to Homebrew Cellar direcory any more:

    $ for x in *.dylib ; do otool -L $x|grep Cell; done

Update a library reference that was not updated by dylibbundler with:

    $ install_name_tool -change /usr/local/Cellar/libvorbis/1.3.4/lib/libvorbis.0.dylib \
    @executable_path/../../libs/libvorbis.0.dylib libvorbisfile.3.dylib
