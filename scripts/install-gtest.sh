
source scripts/common.sh

function install_gtest(){
    if [ -d "$DEPENDENCIES_DIR/gtest" ]; then
        echo " -- gtest already installed"
        return 0
    fi

    gtest_version=1.8.0
    gtest_dir=googletest-release-$gtest_version
    gtest_zip=release-$gtest_version.zip
    wget "https://github.com/google/googletest/archive/$gtest_zip"
    unzip "$gtest_zip"
    rm "$gtest_zip"
    mv "$gtest_dir/googletest" "$DEPENDENCIES_DIR/gtest"
    rm -rf "$gtest_dir"
}

export -f install_gtest
