
source scripts/common.sh

if exe_exists gcc-5; then
    CC=gcc-5
    CXX=g++-5
else
    CC=gcc
    CXX=g++
fi

function has_gcc(){
    exe_exists $CC && exe_exists $CXX
}

function has_right_gcc_version(){
    local gcc_version=`$CC --version | head -1 | grep '5\.[[:digit:]]\.[[:digit:]]'`
    if [ "$gcc_version" ]; then
        return 0
    else
        return 1
    fi
}

function get_gcc_version(){
    local gcc_version_match='[[:digit:]]\.[[:digit:]]\.[[:digit:]]'
    local gcc_version=`$CC --version | grep -o "$gcc_version_match" | head -1`
    echo $gcc_version | grep -o '[[:digit:]]\.[[:digit:]]'
}

function install_gcc_apt(){
    if ! has_gcc; then
        sudo apt-get install --yes gcc g++
    fi

    if ! has_right_gcc_version; then
        gcc_version=$(get_gcc_version)

        sudo apt-add-repository --yes ppa:ubuntu-toolchain-r/test
        sudo apt-get update
        sudo apt-get install --yes gcc-5 g++-5
        sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-$gcc_version 40 --slave /usr/bin/g++ g++ /usr/bin/g++-$gcc_version
        sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 60 --slave /usr/bin/g++ g++ /usr/bin/g++-5
        sudo update-alternatives --auto gcc
    fi
}

function install_gcc_homebrew(){
    if ! has_gcc; then
        brew install gcc49
    fi
}

function install_gcc_dnf(){
    CC=gcc
    CXX=g++
    if ! has_gcc; then
        sudo dnf install gcc gcc-c++
    fi

    if ! has_right_gcc_version; then
        echo " !! Cannot install gcc 5 on this platform (do not know how)" >&2
        exit 1
    fi
}

function install_gcc(){
    if has_right_gcc_version; then
        echo " -- gcc 5 already installed"
        return 0
    fi

    if exe_exists apt-get; then
        install_gcc_apt
    elif exe_exists brew; then
        install_gcc_homebrew
    else
        install_gcc_dnf
    fi
}

export -f install_gcc
